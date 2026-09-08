#include "gltf_importer.hpp"

#include <tempest/algorithm.hpp>
#include <tempest/archetype.hpp>
#include <tempest/asset_database.hpp>
#include <tempest/asset_serializers.hpp>
#include <tempest/asset_type_id.hpp>
#include <tempest/files.hpp>
#include <tempest/int.hpp>
#include <tempest/logger.hpp>
#include <tempest/material.hpp>
#include <tempest/relationship_component.hpp>
#include <tempest/serial.hpp>
#include <tempest/texture.hpp>
#include <tempest/transform_component.hpp>
#include <tempest/transformations.hpp>
#include <tempest/vec3.hpp>
#include <tempest/vertex.hpp>

#include <tempest/filesystem.hpp>

#include <cmath>
#include <tempest/json.hpp>
#include <tempest/string_view.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace tempest::assets
{
    using math::float3;

    using tempest::json_array;
    using tempest::json_document;
    using tempest::json_object;
    using tempest::json_value;
    using tempest::system_allocator;

    namespace
    {
        enum class component_type : uint16_t
        {
            BYTE = 5120,
            UNSIGNED_BYTE = 5121,
            SHORT = 5122,
            UNSIGNED_SHORT = 5123,
            UNSIGNED_INT = 5125,
            FLOAT = 5126,
        };

        enum class accessor_type : uint8_t
        {
            SCALAR = 1,
            VEC2 = 2,
            VEC3 = 3,
            VEC4 = 4,
            MAT2 = 4,
            MAT3 = 9,
            MAT4 = 16,
        };

        struct image_payload
        {
            string file_path;
            vector<byte> data;
            int32_t buffer_view_index = -1;
            string mime_type;
            string name;
        };

        struct buffer_view_payload
        {
            uint32_t buffer_id;
            uint32_t byte_offset;
            uint32_t byte_length;
            uint32_t byte_stride;
        };

        struct accessor_payload
        {
            uint32_t buffer_view{};
            uint32_t buffer_offset{};
            component_type ctype{};
            accessor_type atype{};
            bool normalized{};
            uint32_t count{};
            vector<double> min;
            vector<double> max;
        };

        auto get_metadata(const json_object& obj) -> asset_database::asset_metadata
        {
            asset_database::asset_metadata meta;

            for (const auto& [key, value] : obj)
            {
                if (key == "extensions")
                {
                    continue;
                }
                if (auto str_val = value.as_string(); str_val.has_value())
                {
                    meta.metadata[string(key.data(), key.size())] = string(str_val->data(), str_val->size());
                }
            }

            return meta;
        }

        auto parse_base64(span<const byte> data) -> vector<byte>
        {
            const byte data_delim = static_cast<byte>(',');
            const auto* data_start = find(data.begin(), data.end(), data_delim);
            if (data_start == data.end())
            {
                return {};
            }

            data_start = tempest::next(data_start);

            vector<byte> decoded_data;
            decoded_data.resize(data.size() - (data_start - data.begin()));
            copy_n(data_start, decoded_data.size(), decoded_data.begin());
            return decoded_data;
        }

        auto read_buffer(const json_value& buffer, const optional<tempest::filesystem::path>& dir) -> vector<byte>
        {
            vector<byte> data;

            uint64_t byte_length = 0;
            if (buffer["byteLength"].get(byte_length))
            {
                data.resize(byte_length);
            }

            string_view uri;
            if (buffer["uri"].get(uri))
            {
                if (tempest::starts_with(uri, "data:"))
                {
                    data = parse_base64({reinterpret_cast<const byte*>(uri.data()), uri.size()});
                }
                else
                {
                    if (dir)
                    {
                        auto full_path = *dir / tempest::filesystem::path{uri};
                        data = read_file_to_vector(full_path).value_or(vector<byte>{});
                    }
                    else
                    {
                        data = read_file_to_vector(tempest::filesystem::path{uri}).value_or(vector<byte>{});
                    }
                }
            }

            return data;
        }

        auto read_image(const json_value& img, const optional<tempest::filesystem::path>& dir) -> image_payload
        {
            image_payload payload;

            string_view uri;
            uint64_t buffer_view_index = ~uint64_t{0};
            if (img["uri"].get(uri))
            {
                if (tempest::starts_with(uri, "data:"))
                {
                    // Extract mime type
                    const auto *mime_it = tempest::search(uri, string_view{"image/"});
                    if (mime_it != uri.end())
                    {
                        const auto *mime_end = tempest::search_first_of(string_view{mime_it, uri.end()}, string_view{";,"});
                        payload.mime_type = string{mime_it, mime_end};
                    }

                    payload.data = parse_base64({reinterpret_cast<const byte*>(uri.data()), uri.size()});
                }
                else
                {
                    if (dir)
                    {
                        auto full_path = *dir / tempest::filesystem::path{uri};
                        payload.data = read_file_to_vector(full_path).value_or(vector<byte>{});
                        payload.file_path = full_path.string();
                    }
                    else
                    {
                        payload.data = read_file_to_vector(tempest::filesystem::path{uri}).value_or(vector<byte>{});
                        payload.file_path = string{uri.data(), uri.size()};
                    }
                }
            }
            else if (img["bufferView"].get(buffer_view_index))
            {
                payload.buffer_view_index = static_cast<int32_t>(buffer_view_index);
                if (auto mime = img["mimeType"].as_string(); mime.has_value())
                {
                    payload.mime_type = string{mime->data(), mime->size()};
                }
            }

            string_view name;
            if (img["name"].get(name))
            {
                payload.name = string{name.data(), name.size()};
            }

            return payload;
        }

        auto read_buffer_views(const json_array& buffer_views) -> vector<buffer_view_payload>
        {
            vector<buffer_view_payload> views;

            for (const auto& view : buffer_views)
            {
                buffer_view_payload payload{};
                payload.buffer_id = static_cast<uint32_t>(view["buffer"].as_uint64().value_or(0));
                payload.byte_length = static_cast<uint32_t>(view["byteLength"].as_uint64().value_or(0));

                uint64_t byte_offset = 0;
                if (!view["byteOffset"].get(byte_offset))
                {
                    payload.byte_offset = 0;
                }
                else
                {
                    payload.byte_offset = static_cast<uint32_t>(byte_offset);
                }

                uint64_t byte_stride = 0;
                if (!view["byteStride"].get(byte_stride))
                {
                    payload.byte_stride = 0;
                }
                else
                {
                    payload.byte_stride = static_cast<uint32_t>(byte_stride);
                }

                views.push_back(payload);
            }

            return views;
        }

        auto read_accessors(const json_array& accessors) -> vector<accessor_payload>
        {
            vector<accessor_payload> accs;

            for (const auto& accessor : accessors)
            {
                accessor_payload payload;
                payload.buffer_view = static_cast<uint32_t>(accessor["bufferView"].as_uint64().value_or(0));

                uint64_t buffer_offset = 0;
                if (!accessor["byteOffset"].get(buffer_offset))
                {
                    payload.buffer_offset = 0;
                }
                else
                {
                    payload.buffer_offset = static_cast<uint32_t>(buffer_offset);
                }

                payload.ctype = static_cast<component_type>(accessor["componentType"].as_uint64().value_or(0));

                auto accessor_type = accessor["type"].as_string().value_or("");
                if (accessor_type == "SCALAR")
                {
                    payload.atype = accessor_type::SCALAR;
                }
                else if (accessor_type == "VEC2")
                {
                    payload.atype = accessor_type::VEC2;
                }
                else if (accessor_type == "VEC3")
                {
                    payload.atype = accessor_type::VEC3;
                }
                else if (accessor_type == "VEC4")
                {
                    payload.atype = accessor_type::VEC4;
                }
                else if (accessor_type == "MAT2")
                {
                    payload.atype = accessor_type::MAT2;
                }
                else if (accessor_type == "MAT3")
                {
                    payload.atype = accessor_type::MAT3;
                }
                else if (accessor_type == "MAT4")
                {
                    payload.atype = accessor_type::MAT4;
                }

                if (!accessor["normalized"].get(payload.normalized))
                {
                    payload.normalized = false;
                }

                uint64_t count = 0;
                accessor["count"].get(count);
                payload.count = static_cast<uint32_t>(count);

                auto min = json_array{};
                auto max = json_array{};
                if (accessor["min"].get(min))
                {
                    for (const auto& val : min)
                    {
                        if (auto num = val.as_number(); num.has_value())
                        {
                            payload.min.push_back(*num);
                        }
                    }
                }

                if (accessor["max"].get(max))
                {
                    for (const auto& val : max)
                    {
                        if (auto num = val.as_number(); num.has_value())
                        {
                            payload.max.push_back(*num);
                        }
                    }
                }

                accs.push_back(payload);
            }

            return accs;
        }

        namespace gltf
        {
            inline constexpr uint32_t gltf_sampler_nearest = 9728;
            inline constexpr uint32_t gltf_sampler_linear = 9729;
            inline constexpr uint32_t gltf_sampler_nearest_mipmap_nearest = 9984;
            inline constexpr uint32_t gltf_sampler_linear_mipmap_nearest = 9985;
            inline constexpr uint32_t gltf_sampler_nearest_mipmap_linear = 9986;
            inline constexpr uint32_t gltf_sampler_linear_mipmap_linear = 9987;
        } // namespace gltf

        auto parse_sampler(const json_value& sampler) -> core::sampler_state
        {
            core::sampler_state state;

            uint64_t min_filter = 0;
            uint64_t mag_filter = 0;

            if (!sampler["magFilter"].get(mag_filter))
            {
                state.mag_filter = core::magnify_texture_filter::linear;
            }
            else
            {
                switch (mag_filter)
                {
                case gltf::gltf_sampler_nearest:
                    state.mag_filter = core::magnify_texture_filter::nearest;
                    break;
                case gltf::gltf_sampler_linear:
                    [[fallthrough]];
                default:
                    state.mag_filter = core::magnify_texture_filter::linear;
                    break;
                }
            }

            if (!sampler["minFilter"].get(min_filter))
            {
                state.min_filter = core::minify_texture_filter::linear;
            }
            else
            {
                switch (min_filter)
                {
                case gltf::gltf_sampler_nearest:
                    state.min_filter = core::minify_texture_filter::nearest;
                    break;
                case gltf::gltf_sampler_linear:
                    state.min_filter = core::minify_texture_filter::linear;
                    break;
                case gltf::gltf_sampler_nearest_mipmap_nearest:
                    state.min_filter = core::minify_texture_filter::nearest_mipmap_nearest;
                    break;
                case gltf::gltf_sampler_linear_mipmap_nearest:
                    state.min_filter = core::minify_texture_filter::linear_mipmap_nearest;
                    break;
                case gltf::gltf_sampler_nearest_mipmap_linear:
                    state.min_filter = core::minify_texture_filter::nearest_mipmap_linear;
                    break;
                case gltf::gltf_sampler_linear_mipmap_linear:
                    state.min_filter = core::minify_texture_filter::linear_mipmap_linear;
                    break;
                default:
                    state.min_filter = core::minify_texture_filter::linear;
                    break;
                }
            }

            return state;
        }

        auto process_texture(const image_payload& img, optional<json_value> sampler, core::texture_registry* tex_reg,
                             const flat_unordered_map<uint32_t, vector<byte>>& buffers, asset_database& asset_db,
                             string_view source_path) -> guid
        {
            auto sampler_state = core::sampler_state{};

            if (sampler)
            {
                sampler_state = parse_sampler(*sampler);
            }

            core::texture tex = {
                .sampler = sampler_state,
            };

            span<const byte> image_data =
                img.buffer_view_index < 0 ? img.data : buffers.find(img.buffer_view_index)->second;

            // Load image data
            const bool is_16_bit = stbi_is_16_bit_from_memory(reinterpret_cast<const stbi_uc*>(image_data.data()),
                                                              static_cast<int>(image_data.size())) != 0;
            int width = 0;
            int height = 0;
            int components = 0;
            stbi_info_from_memory(reinterpret_cast<const stbi_uc*>(image_data.data()),
                                  static_cast<int>(image_data.size()), &width, &height, &components);

            tex.width = width;
            tex.height = height;

            const bool is_hdr = stbi_is_hdr_from_memory(reinterpret_cast<const stbi_uc*>(image_data.data()),
                                                        static_cast<int>(image_data.size())) != 0;

            if (is_hdr)
            {
                tex.format = core::texture_format::rgba32_float;
                auto* const data =
                    stbi_loadf_from_memory(reinterpret_cast<const stbi_uc*>(image_data.data()),
                                           static_cast<int>(image_data.size()), &width, &height, &components, 4);
                auto& mip = tex.mips.emplace_back();
                mip.width = width;
                mip.height = height;

                mip.data = vector{
                    reinterpret_cast<const byte*>(data),
                    reinterpret_cast<const byte*>(data) + (static_cast<size_t>(width * height * 4) * sizeof(float)),
                };

                stbi_image_free(data);
            }
            else if (is_16_bit)
            {
                tex.format = core::texture_format::rgba16_unorm;
                auto* const data =
                    stbi_load_16_from_memory(reinterpret_cast<const stbi_uc*>(image_data.data()),
                                             static_cast<int>(image_data.size()), &width, &height, &components, 4);
                auto& mip = tex.mips.emplace_back();
                mip.width = width;
                mip.height = height;

                mip.data = vector{
                    reinterpret_cast<const byte*>(data),
                    reinterpret_cast<const byte*>(data) + (static_cast<size_t>(width * height * 4) * sizeof(uint16_t)),
                };

                stbi_image_free(data);
            }
            else
            {
                tex.format = core::texture_format::rgba8_unorm;
                auto* const data =
                    stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(image_data.data()),
                                          static_cast<int32_t>(image_data.size()), &width, &height, &components, 4);
                auto& mip = tex.mips.emplace_back();
                mip.width = width;
                mip.height = height;

                mip.data = vector{
                    reinterpret_cast<byte*>(data),
                    reinterpret_cast<byte*>(data) + (static_cast<size_t>(width * height * 4) * sizeof(byte)),
                };

                stbi_image_free(data);
            }

            if (img.name.empty())
            {
                auto path = tempest::filesystem::path(img.file_path.c_str()).stem();
                tex.name = path.string().c_str();
            }
            else
            {
                tex.name = img.name;
            }

            serialization::binary_archive blob_ar;
            serialization::serializer<serialization::binary_archive, core::texture>::serialize(blob_ar, tex);
            auto tex_id = tex_reg->register_texture(tempest::move(tex));
            auto tex_blob = blob_ar.read(blob_ar.written_size());
            asset_db.register_asset_with_guid(tex_id, asset_type_id::of<core::texture>(), source_path);
            asset_db.store_blob(tex_id, tex_blob);
            return tex_id;
        }

        struct mesh_process_result
        {
            guid mesh_id;
            int32_t material_idx;
        };

        namespace gltf
        {
            inline constexpr float gltf_default_alpha_cutoff = 0.5F;
            inline constexpr float gltf_default_transmission_factor = 0.0F;
            inline constexpr float gltf_default_normal_scale = 1.0F;
            inline constexpr float gltf_default_occlusion_strength = 1.0F;
            inline constexpr float gltf_default_metallic_factor = 1.0F;
            inline constexpr float gltf_default_roughness_factor = 1.0F;
            inline constexpr float gltf_default_volume_thickness_factor = 0.0F;
            inline constexpr float gltf_default_volume_thickness_attenuation_factor = numeric_limits<float>::infinity();

            inline const math::float3 default_emission_color = {0.0F, 0.0F, 0.0F};
            inline const math::float3 default_thickness_attenuation_color = {1.0F, 1.0F, 1.0F};
            inline const math::float4 default_base_color_factor = {1.0F, 1.0F, 1.0F, 1.0F};
        } // namespace gltf

        auto extract_vec3(const json_value& obj, string_view key, const math::float3& default_value) -> math::float3
        {
            auto arr = json_array{};
            if (obj[key].get(arr) && arr.size() >= 3)
            {
                return math::float3{
                    static_cast<float>(arr[0].as_number().value_or(default_value.x)),
                    static_cast<float>(arr[1].as_number().value_or(default_value.y)),
                    static_cast<float>(arr[2].as_number().value_or(default_value.z)),
                };
            }

            return default_value;
        }

        auto extract_vec4(const json_value& obj, string_view key, const math::float4& default_value) -> math::float4
        {
            auto arr = json_array{};
            if (obj[key].get(arr) && arr.size() >= 4)
            {
                return math::float4{
                    static_cast<float>(arr[0].as_number().value_or(default_value.x)),
                    static_cast<float>(arr[1].as_number().value_or(default_value.y)),
                    static_cast<float>(arr[2].as_number().value_or(default_value.z)),
                    static_cast<float>(arr[3].as_number().value_or(default_value.w)),
                };
            }

            return default_value;
        }

        auto extract_scalar(const json_value& obj, string_view key, float default_value) -> float
        {
            auto num = obj[key].as_number();
            if (num.has_value())
            {
                return static_cast<float>(*num);
            }

            return default_value;
        }

        auto extract_boolean(const json_value& obj, string_view key, bool default_value) -> bool
        {
            auto val = obj[key].as_bool();
            if (val.has_value())
            {
                return *val;
            }

            return default_value;
        }

        auto process_base_material_model(const json_value& mat_json, core::material& result,
                                         const flat_unordered_map<uint64_t, guid>& texture_guids) -> void
        {
            auto pbr = json_object{};
            if (mat_json["pbrMetallicRoughness"].get(pbr))
            {
                result.set_vec4(core::material::base_color_factor_name,
                                extract_vec4(pbr, "baseColorFactor", gltf::default_base_color_factor));
                result.set_scalar(core::material::metallic_factor_name,
                                  extract_scalar(pbr, "metallicFactor", gltf::gltf_default_metallic_factor));
                result.set_scalar(core::material::roughness_factor_name,
                                  extract_scalar(pbr, "roughnessFactor", gltf::gltf_default_roughness_factor));

                auto base_color_texture = json_object{};
                if (pbr["baseColorTexture"].get(base_color_texture))
                {
                    uint64_t texture_index = 0;
                    if (base_color_texture["index"].get(texture_index))
                    {
                        result.set_texture(core::material::base_color_texture_name,
                                           texture_guids.find(texture_index)->second);
                    }
                }

                auto metallic_roughness_texture = json_object{};
                if (pbr["metallicRoughnessTexture"].get(metallic_roughness_texture))
                {
                    uint64_t texture_index = 0;
                    if (metallic_roughness_texture["index"].get(texture_index))
                    {
                        result.set_texture(core::material::metallic_roughness_texture_name,
                                           texture_guids.find(texture_index)->second);
                    }
                }
            }

            auto normal_texture = json_object{};
            if (mat_json["normalTexture"].get(normal_texture))
            {
                uint64_t texture_index = 0;
                if (normal_texture["index"].get(texture_index))
                {
                    result.set_texture(core::material::normal_texture_name, texture_guids.find(texture_index)->second);
                }
                result.set_scalar(core::material::normal_scale_name,
                                  extract_scalar(normal_texture, "scale", gltf::gltf_default_normal_scale));
            }

            auto occlusion_texture = json_object{};
            if (mat_json["occlusionTexture"].get(occlusion_texture))
            {
                uint64_t texture_index = 0;
                if (occlusion_texture["index"].get(texture_index))
                {
                    result.set_texture(core::material::occlusion_texture_name,
                                       texture_guids.find(texture_index)->second);
                }
                result.set_scalar(core::material::occlusion_strength_name,
                                  extract_scalar(occlusion_texture, "strength", gltf::gltf_default_occlusion_strength));
            }

            auto emissive_texture = json_object{};
            if (mat_json["emissiveTexture"].get(emissive_texture))
            {
                uint64_t texture_index = 0;
                if (emissive_texture["index"].get(texture_index))
                {
                    result.set_texture(core::material::emissive_texture_name,
                                       texture_guids.find(texture_index)->second);
                }
            }

            result.set_vec3(core::material::emissive_factor_name,
                            extract_vec3(mat_json, "emissiveFactor", gltf::default_emission_color));

            auto alpha_mode = string_view{};
            if (mat_json["alphaMode"].get(alpha_mode))
            {
                result.set_string(core::material::alpha_mode_name, string{alpha_mode.data(), alpha_mode.size()});
            }
            else
            {
                result.set_string(core::material::alpha_mode_name, "OPAQUE");
            }

            result.set_scalar(core::material::alpha_cutoff_name,
                              extract_scalar(mat_json, "alphaCutoff", gltf::gltf_default_alpha_cutoff));
            result.set_bool(core::material::double_sided_name, extract_boolean(mat_json, "doubleSided", false));
        }

        auto process_khr_materials_tansmission(const json_object& extensions,
                                               const flat_unordered_map<uint64_t, guid>& texture_guids,
                                               core::material& result) -> void
        {
            // Check if KHR_materials_transmission exists
            auto khr_materials_transmission = json_object{};
            if (extensions["KHR_materials_transmission"].get(khr_materials_transmission))
            {
                result.set_string(core::material::alpha_mode_name, "TRANSMISSIVE");

                result.set_scalar(core::material::transmissive_factor_name,
                                  extract_scalar(khr_materials_transmission, "transmissionFactor",
                                                 gltf::gltf_default_transmission_factor));

                auto transmissive_texture = json_object{};
                if (khr_materials_transmission["transmissiveTexture"].get(transmissive_texture))
                {
                    uint64_t texture_index = 0;
                    if (transmissive_texture["index"].get(texture_index))
                    {
                        result.set_texture(core::material::transmissive_texture_name,
                                           texture_guids.find(texture_index)->second);
                    }
                }
            }
        }

        auto process_khr_materials_volume(const json_object& extensions,
                                          const flat_unordered_map<uint64_t, guid>& texture_guids,
                                          core::material& result)
        {
            // Check if KHR_materials_volume exists
            auto khr_materials_volume = json_object{};
            if (extensions["KHR_materials_volume"].get(khr_materials_volume))
            {
                result.set_scalar(core::material::volume_thickness_factor_name,
                                  extract_scalar(khr_materials_volume, "thicknessFactor",
                                                 gltf::gltf_default_volume_thickness_factor));
                result.set_scalar(core::material::volume_attenuation_distance_name,
                                  extract_scalar(khr_materials_volume, "attenuationDistance",
                                                 gltf::gltf_default_volume_thickness_attenuation_factor));
                result.set_vec3(
                    core::material::volume_attenuation_color_name,
                    extract_vec3(khr_materials_volume, "attenuationColor", gltf::default_thickness_attenuation_color));

                auto volume_texture = json_object{};
                if (khr_materials_volume["volumeTexture"].get(volume_texture))
                {
                    uint64_t texture_index = 0;
                    if (volume_texture["index"].get(texture_index))
                    {
                        result.set_texture(core::material::volume_thickness_texture_name,
                                           texture_guids.find(texture_index)->second);
                    }
                }
            }
        }

        auto process_material_extensions(const json_value& mat_json,
                                         const flat_unordered_map<uint64_t, guid>& texture_guids,
                                         core::material& result)
        {
            auto extensions = json_object{};
            if (mat_json["extensions"].get(extensions))
            {
                process_khr_materials_tansmission(extensions, texture_guids, result);
                process_khr_materials_volume(extensions, texture_guids, result);
            }
        }

        auto process_material(const json_value& mat, const flat_unordered_map<uint64_t, guid>& texture_guids,
                              core::material_registry* mat_reg, asset_database& asset_db, string_view source_path)
            -> guid
        {
            auto material = core::material{};

            auto name = string_view{};
            if (mat["name"].get(name))
            {
                material.set_name({name.data(), name.size()});
            }

            process_base_material_model(mat, material, texture_guids);
            process_material_extensions(mat, texture_guids, material);

            serialization::binary_archive blob_ar;
            serialization::serializer<serialization::binary_archive, core::material>::serialize(blob_ar, material);
            auto mat_id = mat_reg->register_material(tempest::move(material));
            auto mat_blob = blob_ar.read(blob_ar.written_size());
            asset_db.register_asset_with_guid(mat_id, asset_type_id::of<core::material>(), source_path);
            asset_db.store_blob(mat_id, mat_blob);
            return mat_id;
        }

        auto process_mesh_positions(const vector<byte>& buffer, const accessor_payload& accessor,
                                    const buffer_view_payload& view, vector<core::vertex>& vertices) -> void
        {
            assert(accessor.ctype == component_type::FLOAT);
            assert(accessor.atype == accessor_type::VEC3);

            vertices.resize(accessor.count);

            for (uint32_t i = 0; i < accessor.count; ++i)
            {
                const auto stride_length = view.byte_stride == 0 ? 3 * sizeof(float) : view.byte_stride;
                const auto offset = view.byte_offset + accessor.buffer_offset + (i * stride_length);
                const auto* const ptr = reinterpret_cast<const float*>(buffer.data() + offset);

                vertices[i].position = math::float3(ptr[0], ptr[1], ptr[2]);
            }
        }

        auto process_mesh_normals(const vector<byte>& buffer, const accessor_payload& accessor,
                                  const buffer_view_payload& view, vector<core::vertex>& vertices) -> void
        {
            assert(accessor.ctype == component_type::FLOAT);
            assert(accessor.atype == accessor_type::VEC3);

            for (uint32_t i = 0; i < accessor.count; ++i)
            {
                const auto stride_length = view.byte_stride == 0 ? 3 * sizeof(float) : view.byte_stride;
                const auto offset = view.byte_offset + accessor.buffer_offset + (i * stride_length);
                const auto* const ptr = reinterpret_cast<const float*>(buffer.data() + offset);

                vertices[i].normal = math::float3(ptr[0], ptr[1], ptr[2]);
            }
        }

        auto process_mesh_uv0(const vector<byte>& buffer, const accessor_payload& accessor,
                              const buffer_view_payload& view, vector<core::vertex>& vertices) -> void
        {
            assert(accessor.atype == accessor_type::VEC2);

            switch (accessor.ctype)
            {
            case component_type::FLOAT: {
                for (uint32_t i = 0; i < accessor.count; ++i)
                {
                    const auto stride_length = view.byte_stride == 0 ? 2 * sizeof(float) : view.byte_stride;
                    const auto offset = view.byte_offset + accessor.buffer_offset + (i * stride_length);
                    const auto* const ptr = reinterpret_cast<const float*>(buffer.data() + offset);

                    vertices[i].uv = math::float2(ptr[0], ptr[1]);
                }
                break;
            }
            case component_type::UNSIGNED_SHORT: {
                assert(accessor.normalized);
                for (uint32_t i = 0; i < accessor.count; ++i)
                {
                    const auto stride_length = view.byte_stride == 0 ? 2 * sizeof(uint16_t) : view.byte_stride;
                    const auto offset = view.byte_offset + accessor.buffer_offset + (i * stride_length);
                    const auto* const ptr = reinterpret_cast<const uint16_t*>(buffer.data() + offset);

                    vertices[i].uv = math::float2(static_cast<float>(ptr[0]) / numeric_limits<uint16_t>::max(),
                                                  static_cast<float>(ptr[1]) / numeric_limits<uint16_t>::max());
                }
                break;
            }
            case component_type::UNSIGNED_BYTE: {
                assert(accessor.normalized);
                for (uint32_t i = 0; i < accessor.count; ++i)
                {
                    const auto stride_length = view.byte_stride == 0 ? 2 * sizeof(uint8_t) : view.byte_stride;
                    const auto offset = view.byte_offset + accessor.buffer_offset + (i * stride_length);
                    const auto* const ptr = reinterpret_cast<const uint8_t*>(buffer.data() + offset);

                    vertices[i].uv = math::float2(static_cast<float>(ptr[0]) / numeric_limits<uint8_t>::max(),
                                                  static_cast<float>(ptr[1]) / numeric_limits<uint8_t>::max());
                }
                break;
            }
            default:
                break;
            }
        }

        auto process_mesh_tangents(const vector<byte>& buffer, const accessor_payload& accessor,
                                   const buffer_view_payload& view, vector<core::vertex>& vertices) -> void
        {
            assert(accessor.ctype == component_type::FLOAT);
            assert(accessor.atype == accessor_type::VEC4);

            for (uint32_t i = 0; i < accessor.count; ++i)
            {
                const auto stride_length = view.byte_stride == 0 ? 4 * sizeof(float) : view.byte_stride;
                const auto offset = view.byte_offset + accessor.buffer_offset + (i * stride_length);
                const auto* const ptr = reinterpret_cast<const float*>(buffer.data() + offset);

                vertices[i].tangent = math::float4(ptr[0], ptr[1], ptr[2], ptr[3]);
            }
        }

        auto process_mesh_color0(const vector<byte>& buffer, const accessor_payload& accessor,
                                 const buffer_view_payload& view, vector<core::vertex>& vertices) -> bool
        {
            assert(accessor.atype == accessor_type::VEC4);

            switch (accessor.ctype)
            {
            case component_type::FLOAT: {
                for (uint32_t i = 0; i < accessor.count; ++i)
                {
                    const auto stride_length = view.byte_stride == 0 ? 4 * sizeof(float) : view.byte_stride;
                    const auto offset = view.byte_offset + accessor.buffer_offset + (i * stride_length);
                    const auto* const ptr = reinterpret_cast<const float*>(buffer.data() + offset);

                    vertices[i].color = math::float4(ptr[0], ptr[1], ptr[2], ptr[3]);
                }
                return true;
            }
            case component_type::UNSIGNED_SHORT: {
                assert(accessor.normalized);
                for (uint32_t i = 0; i < accessor.count; ++i)
                {
                    const auto stride_length = view.byte_stride == 0 ? 4 * sizeof(uint16_t) : view.byte_stride;
                    const auto offset = view.byte_offset + accessor.buffer_offset + (i * stride_length);
                    const auto* const ptr = reinterpret_cast<const uint16_t*>(buffer.data() + offset);

                    vertices[i].color = math::float4(static_cast<float>(ptr[0]) / numeric_limits<uint16_t>::max(),
                                                     static_cast<float>(ptr[1]) / numeric_limits<uint16_t>::max(),
                                                     static_cast<float>(ptr[2]) / numeric_limits<uint16_t>::max(),
                                                     static_cast<float>(ptr[3]) / numeric_limits<uint16_t>::max());
                }
                return true;
            }
            case component_type::UNSIGNED_BYTE: {
                assert(accessor.normalized);
                for (uint32_t i = 0; i < accessor.count; ++i)
                {
                    const auto stride_length = view.byte_stride == 0 ? 4 * sizeof(uint8_t) : view.byte_stride;
                    const auto offset = view.byte_offset + accessor.buffer_offset + (i * stride_length);
                    const auto* const ptr = reinterpret_cast<const uint8_t*>(buffer.data() + offset);

                    vertices[i].color = math::float4(static_cast<float>(ptr[0]) / numeric_limits<uint8_t>::max(),
                                                     static_cast<float>(ptr[1]) / numeric_limits<uint8_t>::max(),
                                                     static_cast<float>(ptr[2]) / numeric_limits<uint8_t>::max(),
                                                     static_cast<float>(ptr[3]) / numeric_limits<uint8_t>::max());
                }
                return true;
            }
            default:
                return false;
            }
        }

        auto process_indices(const vector<byte>& buffer, const accessor_payload& accessor,
                             const buffer_view_payload& view, vector<uint32_t>& indices) -> void
        {
            assert(accessor.ctype == component_type::UNSIGNED_BYTE ||
                   accessor.ctype == component_type::UNSIGNED_SHORT || accessor.ctype == component_type::UNSIGNED_INT);
            assert(accessor.atype == accessor_type::SCALAR);

            indices.resize(accessor.count);

            switch (accessor.ctype)
            {
            case component_type::UNSIGNED_BYTE: {
                const auto stride_length = view.byte_stride == 0 ? sizeof(uint8_t) : view.byte_stride;
                for (uint32_t i = 0; i < accessor.count; ++i)
                {
                    const auto offset = view.byte_offset + accessor.buffer_offset + (i * stride_length);
                    const auto* const ptr = reinterpret_cast<const uint8_t*>(buffer.data() + offset);

                    indices[i] = *ptr;
                }
                break;
            }
            case component_type::UNSIGNED_SHORT: {
                const auto stride_length = view.byte_stride == 0 ? sizeof(uint16_t) : view.byte_stride;
                for (uint32_t i = 0; i < accessor.count; ++i)
                {
                    const auto offset = view.byte_offset + accessor.buffer_offset + (i * stride_length);
                    const auto* const ptr = reinterpret_cast<const uint16_t*>(buffer.data() + offset);

                    indices[i] = *ptr;
                }
                break;
            }
            case component_type::UNSIGNED_INT: {
                const auto stride_length = view.byte_stride == 0 ? sizeof(uint32_t) : view.byte_stride;
                if (stride_length == sizeof(uint32_t))
                {
                    // Fast path for tightly packed indices
                    tempest::copy_n(
                        reinterpret_cast<const uint32_t*>(buffer.data() + view.byte_offset + accessor.buffer_offset),
                        accessor.count, indices.data());
                }
                else
                {
                    for (uint32_t i = 0; i < accessor.count; ++i)
                    {
                        const auto offset = view.byte_offset + accessor.buffer_offset + (i * stride_length);
                        const auto* const ptr = reinterpret_cast<const uint32_t*>(buffer.data() + offset);

                        indices[i] = *ptr;
                    }
                }
                break;
            }
            default:
                break;
            }
        }

        auto process_mesh(const flat_unordered_map<uint32_t, vector<byte>>& buffer_contents, const json_value& prim,
                          span<buffer_view_payload> views, span<accessor_payload> accessors,
                          core::mesh_registry* mesh_reg, asset_database& asset_db, string_view source_path)
            -> mesh_process_result
        {
            mesh_process_result result;

            auto mesh = core::mesh{};

            if (auto attribs = prim["attributes"].as_object(); attribs.has_value())
            {
                if (auto positions = (*attribs)["POSITION"].as_uint64(); positions.has_value())
                {
                    auto accessor_idx = *positions;
                    const auto& accessor = accessors[accessor_idx];
                    const auto& view = views[accessor.buffer_view];
                    const vector<byte>& buffer = buffer_contents.find(view.buffer_id)->second;

                    process_mesh_positions(buffer, accessor, view, mesh.vertices);
                }

                if (auto normals = (*attribs)["NORMAL"].as_uint64(); normals.has_value())
                {
                    auto accessor_idx = *normals;
                    const auto& accessor = accessors[accessor_idx];
                    const auto& view = views[accessor.buffer_view];
                    const vector<byte>& buffer = buffer_contents.find(view.buffer_id)->second;

                    process_mesh_normals(buffer, accessor, view, mesh.vertices);
                    mesh.has_normals = true;
                }

                if (auto uvs = (*attribs)["TEXCOORD_0"].as_uint64(); uvs.has_value())
                {
                    auto accessor_idx = *uvs;
                    const auto& accessor = accessors[accessor_idx];
                    const auto& view = views[accessor.buffer_view];
                    const vector<byte>& buffer = buffer_contents.find(view.buffer_id)->second;

                    process_mesh_uv0(buffer, accessor, view, mesh.vertices);
                }

                if (auto tangents = (*attribs)["TANGENT"].as_uint64(); tangents.has_value())
                {
                    auto accessor_idx = *tangents;
                    const auto& accessor = accessors[accessor_idx];
                    const auto& view = views[accessor.buffer_view];
                    const vector<byte>& buffer = buffer_contents.find(view.buffer_id)->second;

                    process_mesh_tangents(buffer, accessor, view, mesh.vertices);
                    mesh.has_tangents = true;
                }

                if (auto colors = (*attribs)["COLOR_0"].as_uint64(); colors.has_value())
                {
                    auto accessor_idx = *colors;
                    const auto& accessor = accessors[accessor_idx];
                    const auto& view = views[accessor.buffer_view];
                    const vector<byte>& buffer = buffer_contents.find(view.buffer_id)->second;

                    assert(accessor.atype == accessor_type::VEC4);

                    mesh.has_colors = process_mesh_color0(buffer, accessor, view, mesh.vertices);
                }
            }

            if (auto indices = prim["indices"].as_uint64(); indices.has_value())
            {
                auto accessor_idx = *indices;
                const auto& accessor = accessors[accessor_idx];
                const auto& view = views[accessor.buffer_view];
                const vector<byte>& buffer = buffer_contents.find(view.buffer_id)->second;

                assert(accessor.atype == accessor_type::SCALAR);

                mesh.indices.resize(accessor.count);
                process_indices(buffer, accessor, view, mesh.indices);
            }

            if (!mesh.has_normals)
            {
                mesh.compute_normals();
            }

            if (!mesh.has_tangents)
            {
                mesh.compute_tangents();
            }

            serialization::binary_archive blob_ar;
            serialization::serializer<serialization::binary_archive, core::mesh>::serialize(blob_ar, mesh);
            result.mesh_id = mesh_reg->register_mesh(move(mesh));
            auto mesh_blob = blob_ar.read(blob_ar.written_size());
            asset_db.register_asset_with_guid(result.mesh_id, asset_type_id::of<core::mesh>(), source_path);
            asset_db.store_blob(result.mesh_id, mesh_blob);

            if (auto material = prim["material"].as_int64(); material.has_value())
            {
                result.material_idx = static_cast<int32_t>(*material);
            }

            return result;
        }
    } // namespace

    gltf_importer::gltf_importer(core::mesh_registry* mesh_reg, core::texture_registry* texture_reg,
                                 core::material_registry* material_reg) noexcept
        : _mesh_reg{mesh_reg}, _texture_reg{texture_reg}, _material_reg{material_reg}
    {
    }

    auto load_asset_metadata(const json_object& doc, ecs::archetype_registry& registry, ecs::entity ent,
                             asset_database& asset_db) -> void
    {
        auto asset = json_object{};
        if (doc["asset"].get(asset))
        {
            asset_database::asset_metadata meta = get_metadata(asset);
            auto meta_id = asset_db.register_asset_metadata(meta);

            asset_metadata_component meta_comp{
                .metadata_id = meta_id,
            };

            registry.assign(ent, meta_comp);
        }
    }

    auto load_buffer_contents(const json_object& doc, const optional<tempest::filesystem::path>& base_path)
        -> flat_unordered_map<uint32_t, vector<byte>>
    {
        auto buffer_contents = flat_unordered_map<uint32_t, vector<byte>>{};

        auto buffers = json_array{};
        if (doc["buffers"].get(buffers))
        {
            auto buffer_id = 0U;
            for (const auto& buffer : buffers)
            {
                auto buffer_data = read_buffer(buffer, base_path);
                buffer_contents.insert({buffer_id, move(buffer_data)});
                ++buffer_id;
            }
        }

        return buffer_contents;
    }

    auto load_image_contents(const json_object& doc, const optional<tempest::filesystem::path>& base_path)
        -> flat_unordered_map<uint32_t, image_payload>
    {
        auto image_contents = flat_unordered_map<uint32_t, image_payload>{};

        auto images = json_array{};
        if (doc["images"].get(images))
        {
            auto image_id = 0U;
            for (const auto& img : images)
            {
                auto payload = read_image(img, base_path);
                image_contents.insert({image_id, move(payload)});
                ++image_id;
            }
        }

        return image_contents;
    }

    auto process_textures(const json_object& doc, const flat_unordered_map<uint32_t, image_payload>& image_contents,
                          const flat_unordered_map<uint32_t, vector<byte>>& buffer_contents,
                          core::texture_registry* texture_registry, asset_database& asset_db, string_view source_path)
        -> flat_unordered_map<uint64_t, guid>
    {
        auto texture_guids = flat_unordered_map<uint64_t, guid>{};
        auto textures = json_array{};

        if (doc["textures"].get(textures))
        {
            auto texture_id = uint64_t{};
            for (const auto& tex : textures)
            {
                const auto image_id = static_cast<uint32_t>(tex["source"].as_uint64().value_or(0));

                auto sampler = optional<json_value>{nullopt};

                auto sampler_id = uint64_t{};
                if (tex["sampler"].get(sampler_id))
                {
                    auto s = doc["samplers"][sampler_id];
                    if (s.is_valid())
                    {
                        sampler = s;
                    }
                }

                auto guid = process_texture(image_contents.find(image_id)->second, sampler, texture_registry,
                                            buffer_contents, asset_db, source_path);
                texture_guids.insert({texture_id, guid});
                ++texture_id;
            }
        }

        return texture_guids;
    }

    auto process_materials(const json_object& doc, const flat_unordered_map<uint64_t, guid>& texture_guids,
                           core::material_registry* material_registry, asset_database& asset_db,
                           string_view source_path) -> flat_unordered_map<uint32_t, guid>
    {
        auto material_guids = flat_unordered_map<uint32_t, guid>{};
        auto materials = json_array{};
        if (doc["materials"].get(materials))
        {
            auto material_id = 0U;
            for (const auto& mat : materials)
            {
                auto guid = process_material(mat, texture_guids, material_registry, asset_db, source_path);
                material_guids.insert({material_id, guid});

                ++material_id;
            }
        }

        return material_guids;
    }

    struct primitive_info
    {
        guid mesh_id;
        optional<guid> material_id;
    };

    struct mesh_processing_result
    {
        vector<primitive_info> primitives;
        string name;
    };

    auto process_meshes(const json_object& doc, const flat_unordered_map<uint32_t, vector<byte>>& buffer_contents,
                        span<buffer_view_payload> buffer_views, span<accessor_payload> accessors,
                        const flat_unordered_map<uint32_t, guid>& material_guids, core::mesh_registry* mesh_registry,
                        asset_database& asset_db, string_view source_path)
        -> flat_unordered_map<uint32_t, mesh_processing_result>
    {
        auto mesh_primitives = flat_unordered_map<uint32_t, mesh_processing_result>{};
        auto meshes = json_array{};

        if (doc["meshes"].get(meshes))
        {
            auto mesh_idx = 0U;
            for (const auto& mesh : meshes)
            {
                auto primitives = vector<primitive_info>{};

                if (auto prims = mesh["primitives"].as_array(); prims.has_value())
                {
                    for (const auto& prim : *prims)
                    {
                        auto [mesh_id, material_idx] = process_mesh(buffer_contents, prim, buffer_views, accessors,
                                                                    mesh_registry, asset_db, source_path);

                        auto mat_id = optional<guid>{};
                        if (material_idx >= 0)
                        {
                            if (auto it = material_guids.find(material_idx); it != material_guids.end())
                            {
                                mat_id = it->second;
                            }
                        }

                        primitives.push_back(primitive_info{
                            .mesh_id = mesh_id,
                            .material_id = mat_id,
                        });
                    }
                }

                auto result = mesh_processing_result{
                    .primitives = move(primitives),
                    .name = {},
                };

                // Get mesh name
                string_view name;
                if (mesh["name"].get(name))
                {
                    result.name = {name.data(), name.size()};
                }

                mesh_primitives.insert({mesh_idx, move(result)});
                ++mesh_idx;
            }
        }

        return mesh_primitives;
    }

    auto extract_translation(const json_object& node) -> math::vec3<float>
    {
        auto translation = math::float3{0.0F, 0.0F, 0.0F};

        auto translation_json = json_array{};
        if (node["translation"].get(translation_json) && translation_json.size() >= 3)
        {
            translation = math::vec3<float>{static_cast<float>(translation_json[0].as_number().value_or(0.0)),
                                            static_cast<float>(translation_json[1].as_number().value_or(0.0)),
                                            static_cast<float>(translation_json[2].as_number().value_or(0.0))};
        }

        return translation;
    }

    auto extract_rotation(const json_object& node) -> math::vec3<float>
    {
        auto rotation = math::float3{0.0F, 0.0F, 0.0F};

        auto rotation_json = json_array{};
        if (node["rotation"].get(rotation_json) && rotation_json.size() >= 4)
        {
            const auto quat_rot = math::quat<float>{
                static_cast<float>(rotation_json[0].as_number().value_or(0.0)),
                static_cast<float>(rotation_json[1].as_number().value_or(0.0)),
                static_cast<float>(rotation_json[2].as_number().value_or(0.0)),
                static_cast<float>(rotation_json[3].as_number().value_or(1.0)),
            };

            rotation = math::euler(quat_rot);
        }

        return rotation;
    }

    auto extract_scale(const json_object& node) -> math::vec3<float>
    {
        auto scale_json = json_array{};
        auto scale = math::float3{1.0F, 1.0F, 1.0F};

        if (node["scale"].get(scale_json) && scale_json.size() >= 3)
        {
            scale = {
                static_cast<float>(scale_json[0].as_number().value_or(1.0)),
                static_cast<float>(scale_json[1].as_number().value_or(1.0)),
                static_cast<float>(scale_json[2].as_number().value_or(1.0)),
            };
        }

        return scale;
    }

    auto extract_transformation_matrix(const json_object& node, [[maybe_unused]] uint32_t node_id)
        -> optional<ecs::transform_component>
    {
        auto matrix_json = json_array{};
        if (node["matrix"].get(matrix_json) && matrix_json.size() >= 16)
        {
            // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)
            auto transform_matrix = math::mat4<float>(static_cast<float>(matrix_json[0].as_number().value_or(0.0)),
                                                      static_cast<float>(matrix_json[1].as_number().value_or(0.0)),
                                                      static_cast<float>(matrix_json[2].as_number().value_or(0.0)),
                                                      static_cast<float>(matrix_json[3].as_number().value_or(0.0)),
                                                      static_cast<float>(matrix_json[4].as_number().value_or(0.0)),
                                                      static_cast<float>(matrix_json[5].as_number().value_or(0.0)),
                                                      static_cast<float>(matrix_json[6].as_number().value_or(0.0)),
                                                      static_cast<float>(matrix_json[7].as_number().value_or(0.0)),
                                                      static_cast<float>(matrix_json[8].as_number().value_or(0.0)),
                                                      static_cast<float>(matrix_json[9].as_number().value_or(0.0)),
                                                      static_cast<float>(matrix_json[10].as_number().value_or(0.0)),
                                                      static_cast<float>(matrix_json[11].as_number().value_or(0.0)),
                                                      static_cast<float>(matrix_json[12].as_number().value_or(0.0)),
                                                      static_cast<float>(matrix_json[13].as_number().value_or(0.0)),
                                                      static_cast<float>(matrix_json[14].as_number().value_or(0.0)),
                                                      static_cast<float>(matrix_json[15].as_number().value_or(0.0)));
            // NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

            auto translation_vec = math::vec3<float>{};
            auto rotation_quat = math::quat<float>{};
            auto scale_vec = math::vec3<float>{};

            if (!math::decompose(transform_matrix, translation_vec, rotation_quat, scale_vec))
            {
                return ecs::transform_component::identity();
            }

            auto transform = ecs::transform_component::identity();
            transform.position(translation_vec);
            transform.rotation(math::euler(rotation_quat));
            transform.scale(scale_vec);

            return transform;
        }

        return nullopt;
    }

    auto build_entity_relationships(const json_array& nodes, ecs::archetype_registry& registry, ecs::entity root,
                                    const flat_unordered_map<uint32_t, ecs::entity>& node_entities) -> void
    {
        // Apply parent child relationships
        auto node_id = 0U;
        for (const auto& node : nodes)
        {
            auto node_ent = node_entities.find(node_id)->second;

            auto children = json_array{};
            if (node["children"].get(children))
            {
                for (const auto& child : children)
                {
                    auto child_id = static_cast<uint32_t>(child.as_uint64().value_or(0));
                    auto child_ent = node_entities.find(child_id)->second;

                    ecs::create_parent_child_relationship(registry, node_ent, child_ent);
                }
            }

            ++node_id;
        }

        // For each node entity without a parent, assign it to the root entity
        for (const auto& [id, e] : node_entities)
        {
            // Get the relationship
            const auto* rel = registry.try_get<ecs::relationship_component<ecs::entity>>(e);
            if (rel == nullptr || rel->parent == ecs::tombstone)
            {
                ecs::create_parent_child_relationship(registry, root, e);
            }
        }
    }

    auto process_nodes(const json_object& doc,
                       const flat_unordered_map<uint32_t, mesh_processing_result>& mesh_primitives,
                       ecs::archetype_registry& registry, ecs::entity root) -> void
    {
        auto nodes = json_array{};
        if (doc["nodes"].get(nodes))
        {
            // Apply transformations to node, apply child parent relationships to mesh entities and nodes
            auto node_entities = flat_unordered_map<uint32_t, ecs::entity>{};
            auto node_id = 0U;
            for (const auto& node : nodes)
            {
                auto parent_ent = registry.create();

                auto mesh_id = uint64_t{};
                if (node["mesh"].get(mesh_id))
                {
                    auto mesh_prims = mesh_primitives.find(static_cast<uint32_t>(mesh_id));
                    if (mesh_prims != mesh_primitives.end())
                    {
                        for (const auto& prim : mesh_prims->second.primitives)
                        {
                            auto child_mesh_ent = registry.create<core::mesh_component, ecs::transform_component>();
                            registry.replace(child_mesh_ent, core::mesh_component{.mesh_id = prim.mesh_id});
                            registry.replace(child_mesh_ent, ecs::transform_component::identity());

                            if (prim.material_id.has_value())
                            {
                                registry.assign(child_mesh_ent,
                                                core::material_component{.material_id = *prim.material_id});
                            }

                            registry.assign(child_mesh_ent, prefab_tag);
                            ecs::create_parent_child_relationship(registry, parent_ent, child_mesh_ent);
                        }

                        if (!mesh_prims->second.name.empty())
                        {
                            registry.name(parent_ent, mesh_prims->second.name);
                        }
                    }
                }

                // Get the transform
                auto node_obj = node.as_object();
                if (node_obj.has_value())
                {
                    auto transform_opt = extract_transformation_matrix(*node_obj, node_id);
                    auto transform = transform_opt
                                         .or_else([&]() -> optional<ecs::transform_component> {
                                             auto transform = ecs::transform_component::identity();
                                             transform.position(extract_translation(*node_obj));
                                             transform.rotation(extract_rotation(*node_obj));
                                             transform.scale(extract_scale(*node_obj));
                                             return transform;
                                         })
                                         .value();

                    registry.assign(parent_ent, transform);
                }
                else
                {
                    registry.assign(parent_ent, ecs::transform_component::identity());
                }
                registry.assign(parent_ent, prefab_tag);

                node_entities.insert({node_id, parent_ent});
                ++node_id;
            }

            build_entity_relationships(nodes, registry, root, node_entities);
        }
    }

    auto gltf_importer::import(asset_database& asset_db, span<const byte> bytes, ecs::archetype_registry& registry,
                               optional<string_view> path) -> ecs::entity
    {
        auto alloc = system_allocator{};
        auto doc_res = json_document::from_bytes(bytes, alloc);
        if (!doc_res.has_value())
        {
            return ecs::null;
        }

        auto doc_root = doc_res->root().as_object();
        if (!doc_root.has_value())
        {
            return ecs::null;
        }
        const auto& doc = *doc_root;

        auto ent = registry.create();

        optional<tempest::filesystem::path> base_path;
        if (path)
        {
            auto file_path = tempest::filesystem::path(path->data());
            if (file_path.has_parent_path())
            {
                base_path = file_path.parent_path();
            }
        }

        load_asset_metadata(doc, registry, ent, asset_db);

        auto buffer_contents = load_buffer_contents(doc, base_path);
        auto image_contents = load_image_contents(doc, base_path);
        auto buffer_views = read_buffer_views(doc["bufferViews"].as_array().value_or(json_array{}));
        auto accessors = read_accessors(doc["accessors"].as_array().value_or(json_array{}));
        const auto source_path = path.has_value() ? path.value() : string_view{};
        auto texture_guids =
            process_textures(doc, image_contents, buffer_contents, _texture_reg, asset_db, source_path);
        auto material_guids = process_materials(doc, texture_guids, _material_reg, asset_db, source_path);
        auto mesh_primitives = process_meshes(doc, buffer_contents, buffer_views, accessors, material_guids, _mesh_reg,
                                              asset_db, source_path);

        process_nodes(doc, mesh_primitives, registry, ent);

        // If there is only one child, merge it with the root entity
        const auto* ent_rel = registry.try_get<ecs::relationship_component<ecs::entity>>(ent);
        if (ent_rel != nullptr && ent_rel->first_child != ecs::tombstone)
        {
            // There exists at least one child
            const auto child = ent_rel->first_child;
            const auto* child_rel = registry.try_get<ecs::relationship_component<ecs::entity>>(child);
            const auto has_siblings = child_rel->next_sibling != ecs::tombstone;

            // If there are no siblings, copy the components from the parent to the child and delete the parent
            if (!has_siblings)
            {
                // Copy components from parent to child
                // Asset Metadata
                const auto* meta = registry.try_get<asset_metadata_component>(ent);
                if (meta != nullptr)
                {
                    registry.assign_or_replace(child, *meta);
                }

                // Transform
                const auto* transform = registry.try_get<ecs::transform_component>(ent);
                if (transform != nullptr)
                {
                    // Merge the parent and child transforms
                    registry.assign_or_replace(child, *transform);
                }
                else if (!registry.has<ecs::transform_component>(child))
                {
                    // If there is no parent transform, apply a default transform to the child
                    const auto default_tx = ecs::transform_component::identity();
                    registry.assign(child, default_tx);
                }

                // Remove the parent relationship
                auto child_relationship = *child_rel;
                child_relationship.parent = ecs::tombstone;
                registry.replace(child, child_relationship);

                //// Delete the parent
                registry.destroy(ent);

                return child;
            }
        }

        registry.assign_or_replace(ent, prefab_tag);

        return ent;
    }
} // namespace tempest::assets