#include "gltf_importer.hpp"

#include <cstdint>
#include <tempest/algorithm.hpp>
#include <tempest/archetype.hpp>
#include <tempest/asset_database.hpp>
#include <tempest/asset_serializers.hpp>
#include <tempest/asset_type_id.hpp>
#include <tempest/serial.hpp>
#include <tempest/files.hpp>
#include <tempest/logger.hpp>
#include <tempest/material.hpp>
#include <tempest/relationship_component.hpp>
#include <tempest/texture.hpp>
#include <tempest/transform_component.hpp>
#include <tempest/transformations.hpp>
#include <tempest/vec3.hpp>
#include <tempest/vertex.hpp>

#include <filesystem>

#include <cmath>
#include <simdjson.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace tempest::assets
{
    using math::float3;

    namespace sjd = simdjson::dom;

    namespace
    {
        auto log = logger::logger_factory::create({ // NOLINT
            .prefix = "tempest::gltf_importer",
        });

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

        auto get_metadata(const simdjson::dom::object& obj) -> asset_database::asset_metadata
        {
            asset_database::asset_metadata meta;

            for (const auto& [key, value] : obj)
            {
                if (key == "extensions")
                {
                    continue;
                }
                meta.metadata[string(key.data(), key.size())] = value.get_string().value().data();
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

        auto read_buffer(const simdjson::dom::element& buffer, const optional<std::filesystem::path>& dir)
            -> vector<byte>
        {
            vector<byte> data;

            uint64_t byte_length = 0;
            if (buffer["byteLength"].get(byte_length) == simdjson::error_code::SUCCESS)
            {
                data.resize(byte_length);
            }

            std::string_view uri;
            if (buffer["uri"].get(uri) == simdjson::error_code::SUCCESS)
            {
                if (uri.starts_with("data:"))
                {
                    data = parse_base64({reinterpret_cast<const byte*>(uri.data()), uri.size()});
                }
                else
                {
                    if (dir)
                    {
                        auto full_path = (*dir / uri).string();
                        data = core::read_bytes({full_path.c_str(), full_path.size()});
                    }
                    else
                    {
                        data = core::read_bytes({uri.data(), uri.size()});
                    }
                }
            }

            return data;
        }

        auto read_image(const simdjson::dom::element& img, const optional<std::filesystem::path>& dir) -> image_payload
        {
            image_payload payload;

            std::string_view uri;
            uint64_t buffer_view_index = ~static_cast<std::uint64_t>(0);
            if (auto error = img["uri"].get(uri); error == simdjson::error_code::SUCCESS)
            {
                if (uri.starts_with("data:"))
                {
                    // Extract mime type
                    if (auto mime_type = uri.find("image/"))
                    {
                        auto mime_end = uri.find_first_of(";,", mime_type);
                        payload.mime_type = string{uri.data() + mime_type, mime_end - mime_type};
                    }

                    payload.data = parse_base64({reinterpret_cast<const byte*>(uri.data()), uri.size()});
                }
                else
                {
                    if (dir)
                    {
                        auto full_path = (*dir / uri).string();
                        payload.data = core::read_bytes({full_path.c_str(), full_path.size()});
                        payload.file_path = {full_path.c_str(), full_path.size()};
                    }
                    else
                    {
                        payload.data = core::read_bytes({uri.data(), uri.size()});
                        payload.file_path = uri.data();
                    }
                }
            }
            else if (auto error_bv = img["bufferView"].get(buffer_view_index);
                     error_bv == simdjson::error_code::SUCCESS)
            {
                payload.buffer_view_index = static_cast<int32_t>(buffer_view_index);
                payload.mime_type = img["mimeType"].get_string().value().data();
            }

            std::string_view name;
            if (auto error_img = img["name"].get(name); error_img == simdjson::error_code::SUCCESS)
            {
                payload.name = name.data();
            }

            return payload;
        }

        auto read_buffer_views(const simdjson::dom::element& buffer_views) -> vector<buffer_view_payload>
        {
            vector<buffer_view_payload> views;

            for (const auto& view : buffer_views)
            {
                buffer_view_payload payload{};
                payload.buffer_id = static_cast<uint32_t>(view["buffer"].get_uint64().value());
                payload.byte_length = static_cast<uint32_t>(view["byteLength"].get_uint64().value());

                uint64_t byte_offset = 0;
                if (view["byteOffset"].get(byte_offset) != simdjson::error_code::SUCCESS)
                {
                    payload.byte_offset = 0;
                }
                else
                {
                    payload.byte_offset = static_cast<uint32_t>(byte_offset);
                }

                uint64_t byte_stride = 0;
                if (view["byteStride"].get(byte_stride) != simdjson::error_code::SUCCESS)
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

        vector<accessor_payload> read_accessors(const simdjson::dom::element& accessors)
        {
            vector<accessor_payload> accs;

            for (const auto& accessor : accessors)
            {
                accessor_payload payload;
                payload.buffer_view = static_cast<uint32_t>(accessor["bufferView"].get_uint64().value());

                uint64_t buffer_offset = 0;
                if (accessor["byteOffset"].get(buffer_offset) != simdjson::error_code::SUCCESS)
                {
                    payload.buffer_offset = 0;
                }
                else
                {
                    payload.buffer_offset = static_cast<uint32_t>(buffer_offset);
                }

                payload.ctype = static_cast<component_type>(accessor["componentType"].get_uint64().value());

                auto accessor_type = accessor["type"].get_string().value();
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

                if (accessor["normalized"].get(payload.normalized) != simdjson::error_code::SUCCESS)
                {
                    payload.normalized = false;
                }

                uint64_t count = 0;
                (void)accessor["count"].get(count);
                payload.count = static_cast<uint32_t>(count);

                sjd::array min;
                sjd::array max;
                if (accessor["min"].get(min) == simdjson::error_code::SUCCESS)
                {
                    for (const auto& val : min)
                    {
                        payload.min.push_back(val);
                    }
                }

                if (accessor["max"].get(max) == simdjson::error_code::SUCCESS)
                {
                    for (const auto& val : max)
                    {
                        payload.max.push_back(val);
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

        auto parse_sampler(const simdjson::dom::element& sampler) -> core::sampler_state
        {
            core::sampler_state state;

            uint64_t min_filter = 0;
            uint64_t mag_filter = 0;

            if (sampler["magFilter"].get(mag_filter) != simdjson::error_code::SUCCESS)
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

            if (sampler["minFilter"].get(min_filter) != simdjson::error_code::SUCCESS)
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

        auto process_texture(const image_payload& img, optional<simdjson::dom::element> sampler,
                             core::texture_registry* tex_reg, const flat_unordered_map<uint32_t, vector<byte>>& buffers,
                             asset_database& asset_db, string_view source_path)
            -> guid
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
                auto path = std::filesystem::path(img.file_path.c_str()).stem();
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

        auto extract_vec3(const simdjson::dom::object& obj, std::string_view key, const math::float3& default_value)
            -> math::float3
        {
            auto arr = sjd::array{};
            if (obj.at_key(key).get(arr) == simdjson::error_code::SUCCESS)
            {
                return math::float3{
                    static_cast<float>(arr.at(0).get_double()),
                    static_cast<float>(arr.at(1).get_double()),
                    static_cast<float>(arr.at(2).get_double()),
                };
            }

            return default_value;
        }

        auto extract_vec4(const simdjson::dom::object& obj, std::string_view key, const math::float4& default_value)
            -> math::float4
        {
            auto arr = sjd::array{};
            if (obj.at_key(key).get(arr) == simdjson::error_code::SUCCESS)
            {
                return math::float4{
                    static_cast<float>(arr.at(0).get_double()),
                    static_cast<float>(arr.at(1).get_double()),
                    static_cast<float>(arr.at(2).get_double()),
                    static_cast<float>(arr.at(3).get_double()),
                };
            }

            return default_value;
        }

        auto extract_scalar(const simdjson::dom::object& obj, std::string_view key, float default_value) -> float
        {
            auto val = 0.0;
            if (obj.at_key(key).get(val) == simdjson::error_code::SUCCESS)
            {
                return static_cast<float>(val);
            }

            return default_value;
        }

        auto extract_boolean(const simdjson::dom::object& obj, std::string_view key, bool default_value) -> bool
        {
            auto val = false;
            if (obj.at_key(key).get(val) == simdjson::error_code::SUCCESS)
            {
                return val;
            }

            return default_value;
        }

        auto process_base_material_model(const simdjson::dom::element& mat_json, core::material& result,
                                         const flat_unordered_map<uint64_t, guid>& texture_guids) -> void
        {
            auto pbr = sjd::object{};
            if (mat_json["pbrMetallicRoughness"].get(pbr) == simdjson::error_code::SUCCESS)
            {
                result.set_vec4(core::material::base_color_factor_name,
                                extract_vec4(pbr, "baseColorFactor", gltf::default_base_color_factor));
                result.set_scalar(core::material::metallic_factor_name,
                                  extract_scalar(pbr, "metallicFactor", gltf::gltf_default_metallic_factor));
                result.set_scalar(core::material::roughness_factor_name,
                                  extract_scalar(pbr, "roughnessFactor", gltf::gltf_default_roughness_factor));

                auto base_color_texture = sjd::object{};
                if (pbr["baseColorTexture"].get(base_color_texture) == simdjson::error_code::SUCCESS)
                {
                    result.set_texture(core::material::base_color_texture_name,
                                       texture_guids.find(base_color_texture["index"].get_uint64().value())->second);
                }

                auto metallic_roughness_texture = sjd::object{};
                if (pbr["metallicRoughnessTexture"].get(metallic_roughness_texture) == simdjson::error_code::SUCCESS)
                {
                    result.set_texture(
                        core::material::metallic_roughness_texture_name,
                        texture_guids.find(metallic_roughness_texture["index"].get_uint64().value())->second);
                }
            }

            auto normal_texture = sjd::object{};
            if (auto error = mat_json["normalTexture"].get(normal_texture); error == simdjson::error_code::SUCCESS)
            {
                result.set_texture(core::material::normal_texture_name,
                                   texture_guids.find(normal_texture["index"].get_uint64().value())->second);
                result.set_scalar(core::material::normal_scale_name,
                                  extract_scalar(normal_texture, "scale", gltf::gltf_default_normal_scale));
            }

            auto occlusion_texture = sjd::object{};
            if (auto error = mat_json["occlusionTexture"].get(occlusion_texture);
                error == simdjson::error_code::SUCCESS)
            {
                result.set_texture(core::material::occlusion_texture_name,
                                   texture_guids.find(occlusion_texture["index"].get_uint64().value())->second);
                result.set_scalar(core::material::occlusion_strength_name,
                                  extract_scalar(occlusion_texture, "strength", gltf::gltf_default_occlusion_strength));
            }

            auto emissive_texture = sjd::object{};
            if (auto error = mat_json["emissiveTexture"].get(emissive_texture); error == simdjson::error_code::SUCCESS)
            {
                result.set_texture(core::material::emissive_texture_name,
                                   texture_guids.find(emissive_texture["index"].get_uint64().value())->second);
            }

            result.set_vec3(core::material::emissive_factor_name,
                            extract_vec3(mat_json, "emissiveFactor", gltf::default_emission_color));

            auto alpha_mode = std::string_view{};
            if (mat_json["alphaMode"].get(alpha_mode) == simdjson::error_code::SUCCESS)
            {
                result.set_string(core::material::alpha_mode_name, {alpha_mode.data(), alpha_mode.size()});
            }
            else
            {
                result.set_string(core::material::alpha_mode_name, "OPAQUE");
            }

            result.set_scalar(core::material::alpha_cutoff_name,
                              extract_scalar(mat_json, "alphaCutoff", gltf::gltf_default_alpha_cutoff));
            result.set_bool(core::material::double_sided_name, extract_boolean(mat_json, "doubleSided", false));
        }

        auto process_khr_materials_tansmission(const simdjson::dom::object& extensions,
                                               const flat_unordered_map<uint64_t, guid>& texture_guids,
                                               core::material& result) -> void
        {
            // Check if KHR_materials_transmission exists
            auto khr_materials_transmission = sjd::object{};
            if (extensions["KHR_materials_transmission"].get(khr_materials_transmission) ==
                simdjson::error_code::SUCCESS)
            {
                result.set_string(core::material::alpha_mode_name, "TRANSMISSIVE");

                result.set_scalar(core::material::transmissive_factor_name,
                                  extract_scalar(khr_materials_transmission, "transmissionFactor",
                                                 gltf::gltf_default_transmission_factor));

                sjd::object transmissive_texture;
                if (khr_materials_transmission["transmissiveTexture"].get(transmissive_texture) ==
                    simdjson::error_code::SUCCESS)
                {
                    result.set_texture(core::material::transmissive_texture_name,
                                       texture_guids.find(transmissive_texture["index"].get_uint64().value())->second);
                }
            }
        }

        auto process_khr_materials_volume(const simdjson::dom::object& extensions,
                                          const flat_unordered_map<uint64_t, guid>& texture_guids,
                                          core::material& result)
        {
            // Check if KHR_materials_volume exists
            auto khr_materials_volume = sjd::object{};
            if (extensions["KHR_materials_volume"].get(khr_materials_volume) == simdjson::error_code::SUCCESS)
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

                sjd::object volume_texture;
                if (khr_materials_volume["volumeTexture"].get(volume_texture) == simdjson::error_code::SUCCESS)
                {
                    result.set_texture(core::material::volume_thickness_texture_name,
                                       texture_guids.find(volume_texture["index"].get_uint64().value())->second);
                }
            }
        }

        auto process_material_extensions(const simdjson::dom::element& mat_json,
                                         const flat_unordered_map<uint64_t, guid>& texture_guids,
                                         core::material& result)
        {
            sjd::object extensions;
            if (mat_json["extensions"].get(extensions) == simdjson::error_code::SUCCESS)
            {
                process_khr_materials_tansmission(extensions, texture_guids, result);
                process_khr_materials_volume(extensions, texture_guids, result);
            }
        }

        auto process_material(const simdjson::dom::element& mat,
                              const flat_unordered_map<uint64_t, guid>& texture_guids, core::material_registry* mat_reg,
                              asset_database& asset_db, string_view source_path)
            -> guid
        {
            auto material = core::material{};

            auto name = std::string_view{};
            if (auto error = mat["name"].get(name); error == simdjson::error_code::SUCCESS)
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

        auto process_mesh(const flat_unordered_map<uint32_t, vector<byte>>& buffer_contents,
                          const simdjson::dom::element& prim, span<buffer_view_payload> views,
                          span<accessor_payload> accessors, core::mesh_registry* mesh_reg,
                          asset_database& asset_db, string_view source_path) -> mesh_process_result
        {
            mesh_process_result result;

            auto mesh = core::mesh{};

            sjd::object attribs;
            if (prim["attributes"].get(attribs) == simdjson::error_code::SUCCESS)
            {
                if (auto positions = attribs["POSITION"].get_uint64();
                    positions.error() == simdjson::error_code::SUCCESS)
                {
                    auto accessor_idx = positions.value();
                    const auto& accessor = accessors[accessor_idx];
                    const auto& view = views[accessor.buffer_view];
                    const vector<byte>& buffer = buffer_contents.find(view.buffer_id)->second;

                    process_mesh_positions(buffer, accessor, view, mesh.vertices);
                }

                if (auto normals = attribs["NORMAL"].get_uint64(); normals.error() == simdjson::error_code::SUCCESS)
                {
                    auto accessor_idx = normals.value();
                    const auto& accessor = accessors[accessor_idx];
                    const auto& view = views[accessor.buffer_view];
                    const vector<byte>& buffer = buffer_contents.find(view.buffer_id)->second;

                    process_mesh_normals(buffer, accessor, view, mesh.vertices);
                }

                if (auto uvs = attribs["TEXCOORD_0"].get_uint64(); uvs.error() == simdjson::error_code::SUCCESS)
                {
                    auto accessor_idx = uvs.value();
                    const auto& accessor = accessors[accessor_idx];
                    const auto& view = views[accessor.buffer_view];
                    const vector<byte>& buffer = buffer_contents.find(view.buffer_id)->second;

                    process_mesh_uv0(buffer, accessor, view, mesh.vertices);
                }

                if (auto tangents = attribs["TANGENT"].get_uint64(); tangents.error() == simdjson::error_code::SUCCESS)
                {
                    auto accessor_idx = tangents.value();
                    const auto& accessor = accessors[accessor_idx];
                    const auto& view = views[accessor.buffer_view];
                    const vector<byte>& buffer = buffer_contents.find(view.buffer_id)->second;

                    process_mesh_tangents(buffer, accessor, view, mesh.vertices);
                }

                if (auto colors = attribs["COLOR_0"].get_uint64(); colors.error() == simdjson::error_code::SUCCESS)
                {
                    auto accessor_idx = colors.value();
                    const auto& accessor = accessors[accessor_idx];
                    const auto& view = views[accessor.buffer_view];
                    const vector<byte>& buffer = buffer_contents.find(view.buffer_id)->second;

                    assert(accessor.atype == accessor_type::VEC4);

                    mesh.has_colors = process_mesh_color0(buffer, accessor, view, mesh.vertices);
                }
            }

            if (auto indices = prim["indices"].get_uint64(); indices.error() == simdjson::error_code::SUCCESS)
            {
                auto accessor_idx = indices.value();
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

            if (auto material = prim["material"].get_int64(); material.error() == simdjson::error_code::SUCCESS)
            {
                result.material_idx = static_cast<int32_t>(material.value());
            }

            return result;
        }
    } // namespace

    gltf_importer::gltf_importer(core::mesh_registry* mesh_reg, core::texture_registry* texture_reg,
                                 core::material_registry* material_reg) noexcept
        : _mesh_reg{mesh_reg}, _texture_reg{texture_reg}, _material_reg{material_reg}
    {
    }

    auto load_asset_metadata(const sjd::object& doc, ecs::archetype_registry& registry, ecs::archetype_entity ent,
                             asset_database& asset_db) -> void
    {
        auto asset = sjd::object{};
        if (auto error = doc["asset"].get(asset); error == simdjson::SUCCESS)
        {
            asset_database::asset_metadata meta = get_metadata(asset);
            auto meta_id = asset_db.register_asset_metadata(meta);

            asset_metadata_component meta_comp{
                .metadata_id = meta_id,
            };

            registry.assign(ent, meta_comp);
        }
    }

    auto load_buffer_contents(const sjd::object& doc, const optional<std::filesystem::path>& base_path)
        -> flat_unordered_map<uint32_t, vector<byte>>
    {
        auto buffer_contents = flat_unordered_map<uint32_t, vector<byte>>{};

        auto buffers = sjd::array{};
        if (auto error = doc["buffers"].get(buffers); error == simdjson::error_code::SUCCESS)
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

    auto load_image_contents(const sjd::object& doc, const optional<std::filesystem::path>& base_path)
        -> flat_unordered_map<uint32_t, image_payload>
    {
        auto image_contents = flat_unordered_map<uint32_t, image_payload>{};

        auto images = sjd::array{};
        if (auto error = doc["images"].get(images); error == simdjson::error_code::SUCCESS)
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

    auto process_textures(const sjd::object& doc, const flat_unordered_map<uint32_t, image_payload>& image_contents,
                          const flat_unordered_map<uint32_t, vector<byte>>& buffer_contents,
                          core::texture_registry* texture_registry,
                          asset_database& asset_db, string_view source_path) -> flat_unordered_map<uint64_t, guid>
    {
        auto texture_guids = flat_unordered_map<uint64_t, guid>{};
        sjd::array textures;

        if (auto error = doc["textures"].get(textures); error == simdjson::SUCCESS)
        {
            auto texture_id = uint64_t{};
            for (const auto& tex : textures)
            {
                const auto image_id = static_cast<uint32_t>(tex["source"].get_uint64().value());

                auto sampler = optional<simdjson::dom::element>{nullopt};

                auto sampler_id = uint64_t{};
                if (tex["sampler"].get(sampler_id) == simdjson::error_code::SUCCESS)
                {
                    sampler = doc.at_key("samplers").get_array().at(sampler_id).value();
                }

                auto guid =
                    process_texture(image_contents.find(image_id)->second, sampler, texture_registry, buffer_contents,
                                    asset_db, source_path);
                texture_guids.insert({texture_id, guid});
                ++texture_id;
            }
        }

        return texture_guids;
    }

    auto process_materials(const sjd::object& doc, const flat_unordered_map<uint64_t, guid>& texture_guids,
                           core::material_registry* material_registry,
                           asset_database& asset_db, string_view source_path) -> flat_unordered_map<uint32_t, guid>
    {
        auto material_guids = flat_unordered_map<uint32_t, guid>{};
        sjd::array materials;
        if (auto error = doc["materials"].get(materials); error == simdjson::error_code::SUCCESS)
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

    struct mesh_processing_result
    {
        vector<ecs::archetype_entity> prim_entities;
        string name;
    };

    auto process_meshes(const sjd::object& doc, const flat_unordered_map<uint32_t, vector<byte>>& buffer_contents,
                        span<buffer_view_payload> buffer_views, span<accessor_payload> accessors,
                        const flat_unordered_map<uint32_t, guid>& material_guids, core::mesh_registry* mesh_registry,
                        ecs::archetype_registry& registry,
                        asset_database& asset_db, string_view source_path) -> flat_unordered_map<uint32_t, mesh_processing_result>
    {
        auto mesh_primitives = flat_unordered_map<uint32_t, mesh_processing_result>{};
        auto meshes = sjd::array{};

        if (auto error = doc["meshes"].get(meshes); error == simdjson::SUCCESS)
        {
            auto mesh_idx = 0U;
            for (const auto& mesh : meshes)
            {
                auto primitives = vector<ecs::archetype_entity>{};

                for (const auto& prim : mesh["primitives"])
                {
                    auto [mesh_id, material_idx] =
                        process_mesh(buffer_contents, prim, buffer_views, accessors, mesh_registry,
                                     asset_db, source_path);

                    const auto mesh_comp = core::mesh_component{
                        .mesh_id = mesh_id,
                    };

                    auto prim_ent = registry.create<core::mesh_component, ecs::transform_component>();
                    const auto default_tx = ecs::transform_component::identity();

                    registry.replace(prim_ent, mesh_comp);
                    registry.replace(prim_ent, default_tx);

                    if (material_idx >= 0)
                    {
                        const auto mat_comp = core::material_component{
                            .material_id = material_guids.find(material_idx)->second,
                        };

                        registry.assign(prim_ent, mat_comp);
                    }

                    registry.assign(prim_ent, prefab_tag);

                    primitives.push_back(prim_ent);
                }

                mesh_processing_result result = {
                    .prim_entities = move(primitives),
                    .name = {},
                };

                // Get mesh name
                std::string_view name;
                if (auto mesh_error = mesh["name"].get(name); mesh_error == simdjson::SUCCESS)
                {
                    result.name = {name.data(), name.size()};
                }

                mesh_primitives.insert({mesh_idx, result});
                ++mesh_idx;
            }
        }

        return mesh_primitives;
    }

    auto extract_translation(const simdjson::dom::object& node) -> math::vec3<float>
    {
        auto translation = math::float3{0.0F, 0.0F, 0.0F};

        sjd::array translation_json;
        if (node["translation"].get(translation_json) == simdjson::error_code::SUCCESS)
        {
            translation = math::vec3<float>{static_cast<float>(translation_json.at(0).get_double().value()),
                                            static_cast<float>(translation_json.at(1).get_double().value()),
                                            static_cast<float>(translation_json.at(2).get_double().value())};
        }

        return translation;
    }

    auto extract_rotation(const simdjson::dom::object& node) -> math::vec3<float>
    {
        auto rotation = math::float3{0.0F, 0.0F, 0.0F};

        auto rotation_json = sjd::array{};
        if (node["rotation"].get(rotation_json) == simdjson::error_code::SUCCESS)
        {
            auto quat_rot = math::quat<float>{
                static_cast<float>(rotation_json.at(0).get_double().value()),
                static_cast<float>(rotation_json.at(1).get_double().value()),
                static_cast<float>(rotation_json.at(2).get_double().value()),
                static_cast<float>(rotation_json.at(3).get_double().value()),
            };

            auto rot_mat = math::as_mat4(quat_rot);
            const auto* const rot_mat_ptr = rot_mat.data;

            auto rot_x = numeric_limits<float>::quiet_NaN();
            auto rot_z = numeric_limits<float>::quiet_NaN();

            // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)
            const auto rot_y = std::asin(math::clamp(rot_mat_ptr[8], -1.0F, 1.0F));
            if (std::abs(rot_mat_ptr[8]) < 1.0F - std::numeric_limits<float>::epsilon())
            {
                rot_x = std::atan2(-rot_mat_ptr[9], rot_mat_ptr[10]);
                rot_z = std::atan2(-rot_mat_ptr[4], rot_mat_ptr[0]);
            }
            else
            {
                rot_x = std::atan2(rot_mat_ptr[6], rot_mat_ptr[5]);
                rot_z = 0;
            }
            // NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

            rotation = {rot_x, rot_y, rot_z};
        }

        return rotation;
    }

    auto extract_scale(const simdjson::dom::object& node) -> math::vec3<float>
    {
        auto scale_json = sjd::array{};
        auto scale = math::float3{1.0F, 1.0F, 1.0F};

        if (node["scale"].get(scale_json) == simdjson::error_code::SUCCESS)
        {
            scale = {
                static_cast<float>(scale_json.at(0).get_double().value()),
                static_cast<float>(scale_json.at(1).get_double().value()),
                static_cast<float>(scale_json.at(2).get_double().value()),
            };
        }

        return scale;
    }

    auto extract_transformation_matrix(const simdjson::dom::object& node, uint32_t node_id)
        -> optional<ecs::transform_component>
    {
        auto matrix_json = sjd::array{};
        if (node["matrix"].get(matrix_json) == simdjson::error_code::SUCCESS)
        {
            // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)
            auto transform_matrix = math::mat4<float>(static_cast<float>(matrix_json.at(0).get_double().value()),
                                                      static_cast<float>(matrix_json.at(1).get_double().value()),
                                                      static_cast<float>(matrix_json.at(2).get_double().value()),
                                                      static_cast<float>(matrix_json.at(3).get_double().value()),
                                                      static_cast<float>(matrix_json.at(4).get_double().value()),
                                                      static_cast<float>(matrix_json.at(5).get_double().value()),
                                                      static_cast<float>(matrix_json.at(6).get_double().value()),
                                                      static_cast<float>(matrix_json.at(7).get_double().value()),
                                                      static_cast<float>(matrix_json.at(8).get_double().value()),
                                                      static_cast<float>(matrix_json.at(9).get_double().value()),
                                                      static_cast<float>(matrix_json.at(10).get_double().value()),
                                                      static_cast<float>(matrix_json.at(11).get_double().value()),
                                                      static_cast<float>(matrix_json.at(12).get_double().value()),
                                                      static_cast<float>(matrix_json.at(13).get_double().value()),
                                                      static_cast<float>(matrix_json.at(14).get_double().value()),
                                                      static_cast<float>(matrix_json.at(15).get_double().value()));
            // NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

            auto translation_vec = math::vec3<float>{};
            auto rotation_quat = math::quat<float>{};
            auto scale_vec = math::vec3<float>{};

            if (!math::decompose(transform_matrix, translation_vec, rotation_quat, scale_vec))
            {
                log->warn("Failed to decompose transform matrix for node {}, using identity transform instead",
                          node_id);

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

    auto build_entity_relationships(const sjd::array& nodes, ecs::archetype_registry& registry,
                                    ecs::archetype_entity root,
                                    const flat_unordered_map<uint32_t, ecs::archetype_entity>& node_entities) -> void
    {
        // Apply parent child relationships
        auto node_id = 0U;
        for (const auto& node : nodes)
        {
            auto node_ent = node_entities.find(node_id)->second;

            sjd::array children;
            if (node["children"].get(children) == simdjson::error_code::SUCCESS)
            {
                for (const auto& child : children)
                {
                    auto child_id = static_cast<uint32_t>(child.get_uint64().value());
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
            const auto* rel = registry.try_get<ecs::relationship_component<ecs::archetype_entity>>(e);
            if (rel == nullptr || rel->parent == ecs::tombstone)
            {
                ecs::create_parent_child_relationship(registry, root, e);
            }
        }
    }

    auto process_nodes(const sjd::object& doc,
                       const flat_unordered_map<uint32_t, mesh_processing_result>& mesh_primitives,
                       ecs::archetype_registry& registry, ecs::archetype_entity root) -> void
    {
        auto nodes = sjd::array{};
        if (auto error = doc["nodes"].get(nodes); error == simdjson::error_code::SUCCESS)
        {
            // Apply transformations to node, apply child parent relationships to mesh entities and nodes
            auto node_entities = flat_unordered_map<uint32_t, ecs::archetype_entity>{};
            auto node_id = 0U;
            for (const auto& node : nodes)
            {
                auto parent_ent = registry.create();

                auto mesh_id = uint64_t{};
                if (node["mesh"].get(mesh_id) == simdjson::error_code::SUCCESS)
                {
                    auto mesh_prims = mesh_primitives.find(static_cast<uint32_t>(mesh_id));
                    if (mesh_prims != mesh_primitives.end())
                    {
                        span<const ecs::archetype_entity> mesh_entities = mesh_prims->second.prim_entities;
                        for (const auto& mesh_ent : mesh_entities)
                        {
                            auto dup_mesh = registry.duplicate(mesh_ent);
                            ecs::create_parent_child_relationship(registry, parent_ent, dup_mesh);
                        }

                        if (!mesh_prims->second.name.empty())
                        {
                            registry.name(parent_ent, mesh_prims->second.name);
                        }
                    }
                }

                // Get the transform
                auto transform_opt = extract_transformation_matrix(node, node_id);
                auto transform = transform_opt
                                     .or_else([&]() -> optional<ecs::transform_component> {
                                         auto transform = ecs::transform_component::identity();
                                         transform.position(extract_translation(node));
                                         transform.rotation(extract_rotation(node));
                                         transform.scale(extract_scale(node));
                                         return transform;
                                     })
                                     .value();

                registry.assign(parent_ent, transform);
                registry.assign(parent_ent, prefab_tag);

                node_entities.insert({node_id, parent_ent});
                ++node_id;
            }

            build_entity_relationships(nodes, registry, root, node_entities);
        }
    }

    auto gltf_importer::import(asset_database& asset_db, span<const byte> bytes, ecs::archetype_registry& registry,
                               optional<string_view> path) -> ecs::archetype_entity
    {
        simdjson::dom::parser parser;
        simdjson::padded_string padded(reinterpret_cast<const char*>(bytes.data()), bytes.size());
        auto parse_result = parser.parse(padded);

        if (parse_result.error() != simdjson::error_code::SUCCESS)
        {
            return ecs::null;
        }

        auto ent = registry.create();
        auto doc = parse_result.get_object();

        optional<std::filesystem::path> base_path;
        if (path)
        {
            auto file_path = std::filesystem::path(path->data());
            if (file_path.has_parent_path())
            {
                base_path = file_path.parent_path();
            }
        }

        load_asset_metadata(doc.value(), registry, ent, asset_db);

        auto buffer_contents = load_buffer_contents(doc.value(), base_path);
        auto image_contents = load_image_contents(doc.value(), base_path);
        auto buffer_views = read_buffer_views(doc.at_key("bufferViews"));
        auto accessors = read_accessors(doc.at_key("accessors"));
        const auto source_path = path.has_value() ? path.value() : string_view{};
        auto texture_guids = process_textures(doc.value(), image_contents, buffer_contents, _texture_reg,
                                              asset_db, source_path);
        auto material_guids = process_materials(doc.value(), texture_guids, _material_reg, asset_db, source_path);
        auto mesh_primitives =
            process_meshes(doc.value(), buffer_contents, buffer_views, accessors, material_guids, _mesh_reg, registry,
                           asset_db, source_path);

        process_nodes(doc.value(), mesh_primitives, registry, ent);

        // If there is only one child, merge it with the root entity
        auto* ent_rel = registry.try_get<ecs::relationship_component<ecs::archetype_entity>>(ent);
        if (ent_rel != nullptr && ent_rel->first_child != ecs::tombstone)
        {
            // There exists at least one child
            const auto child = ent_rel->first_child;
            const auto* child_rel = registry.try_get<ecs::relationship_component<ecs::archetype_entity>>(child);
            const auto has_siblings = child_rel->next_sibling != ecs::tombstone;

            // If there are no siblings, copy the components from the parent to the child and delete the parent
            if (!has_siblings)
            {
                // Copy components from parent to child
                // Asset Metadata
                auto* meta = registry.try_get<asset_metadata_component>(ent);
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