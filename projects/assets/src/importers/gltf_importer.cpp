#include "gltf_importer.hpp"

#include <tempest/algorithm.hpp>
#include <tempest/asset_database.hpp>
#include <tempest/files.hpp>
#include <tempest/relationship_component.hpp>
#include <tempest/transform_component.hpp>
#include <tempest/transformations.hpp>
#include <tempest/vertex.hpp>

#include <filesystem>

#include <simdjson.h>
#include <stb_image.h>

namespace tempest::assets
{
    namespace sjd = simdjson::dom;

    namespace
    {
        enum class component_type : uint32_t
        {
            BYTE = 5120,
            UNSIGNED_BYTE = 5121,
            SHORT = 5122,
            UNSIGNED_SHORT = 5123,
            UNSIGNED_INT = 5125,
            FLOAT = 5126,
        };

        enum class accessor_type : uint32_t
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
            uint32_t buffer_view;
            uint32_t buffer_offset;
            component_type ctype;
            accessor_type atype;
            bool normalized;
            uint32_t count;
            vector<double> min;
            vector<double> max;
        };

        asset_database::asset_metadata get_metadata(const simdjson::dom::object& obj)
        {
            asset_database::asset_metadata meta;

            for (const auto& [key, value] : obj)
            {
                meta.metadata[key.data()] = value.get_string().value().data();
            }

            return meta;
        }

        vector<byte> parse_base64(span<const byte> data)
        {
            const byte data_delim = static_cast<byte>(',');
            auto data_start = find(data.begin(), data.end(), data_delim);
            if (data_start == data.end())
            {
                return {};
            }

            ++data_start;

            vector<byte> decoded_data;
            decoded_data.resize(data.size() - (data_start - data.begin()));
            copy_n(data_start, decoded_data.size(), decoded_data.begin());
            return decoded_data;
        }

        vector<byte> read_buffer(const simdjson::dom::element& buffer, const optional<std::filesystem::path>& dir)
        {
            vector<byte> data;

            uint64_t byte_length;
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
                        data = core::read_bytes(uri.data());
                    }
                }
            }

            return data;
        }

        image_payload read_image(const simdjson::dom::element& img, const optional<std::filesystem::path>& dir)
        {
            image_payload payload;

            std::string_view uri;
            uint64_t buffer_view_index = -1;
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
                        payload.data = core::read_bytes(uri.data());
                        payload.file_path = uri.data();
                    }
                }
            }
            else if (auto error = img["bufferView"].get(buffer_view_index); error == simdjson::error_code::SUCCESS)
            {
                payload.buffer_view_index = buffer_view_index;
                payload.mime_type = img["mimeType"].get_string().value().data();
            }

            std::string_view name;
            if (auto error = img["name"].get(name); error == simdjson::error_code::SUCCESS)
            {
                payload.name = name.data();
            }

            return payload;
        }

        vector<buffer_view_payload> read_buffer_views(const simdjson::dom::element& buffer_views)
        {
            vector<buffer_view_payload> views;

            for (const auto& view : buffer_views)
            {
                buffer_view_payload payload;
                payload.buffer_id = view["buffer"].get_uint64().value();
                payload.byte_length = view["byteLength"].get_uint64().value();

                uint64_t byte_offset;
                if (view["byteOffset"].get(byte_offset))
                {
                    payload.byte_offset = 0;
                }
                else
                {
                    payload.byte_offset = static_cast<uint32_t>(byte_offset);
                }

                uint64_t byte_stride;
                if (view["byteStride"].get(byte_stride))
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
                payload.buffer_view = accessor["bufferView"].get_uint64().value();

                uint64_t buffer_offset;
                if (accessor["byteOffset"].get(buffer_offset))
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

                if (accessor["normalized"].get(payload.normalized))
                {
                    payload.normalized = false;
                }

                uint64_t count;
                accessor["count"].get(count);
                payload.count = static_cast<uint32_t>(count);

                sjd::array min, max;
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

        guid process_texture(const image_payload& img, optional<const simdjson::dom::element&> sampler,
                             core::texture_registry* tex_reg, span<const buffer_view_payload> views,
                             const flat_unordered_map<uint32_t, vector<byte>>& buffers)
        {
            core::sampler_state state;

            if (sampler)
            {
                uint64_t min_filter, mag_filter;

                if ((*sampler)["magFilter"].get(mag_filter))
                {
                    state.mag_filter = core::magnify_texture_filter::LINEAR;
                }
                else
                {
                    switch (mag_filter)
                    {
                    case 9728:
                        state.mag_filter = core::magnify_texture_filter::NEAREST;
                        break;
                    case 9729:
                        state.mag_filter = core::magnify_texture_filter::LINEAR;
                        break;
                    default:
                        state.mag_filter = core::magnify_texture_filter::LINEAR;
                        break;
                    }
                }

                if ((*sampler)["minFilter"].get(min_filter))
                {
                    state.min_filter = core::minify_texture_filter::LINEAR;
                }
                else
                {
                    switch (min_filter)
                    {
                    case 9728:
                        state.min_filter = core::minify_texture_filter::NEAREST;
                        break;
                    case 9729:
                        state.min_filter = core::minify_texture_filter::LINEAR;
                        break;
                    case 9984:
                        state.min_filter = core::minify_texture_filter::NEAREST_MIPMAP_NEAREST;
                        break;
                    case 9985:
                        state.min_filter = core::minify_texture_filter::LINEAR_MIPMAP_NEAREST;
                        break;
                    case 9986:
                        state.min_filter = core::minify_texture_filter::NEAREST_MIPMAP_LINEAR;
                        break;
                    case 9987:
                        state.min_filter = core::minify_texture_filter::LINEAR_MIPMAP_LINEAR;
                        break;
                    default:
                        state.min_filter = core::minify_texture_filter::LINEAR;
                        break;
                    }
                }

                uint64_t wrap_s, wrap_t;
                if ((*sampler)["wrapS"].get(wrap_s))
                {
                    state.wrap_s = core::texture_wrap_mode::REPEAT;
                }
                else
                {
                    switch (wrap_s)
                    {
                    case 33071:
                        state.wrap_s = core::texture_wrap_mode::CLAMP_TO_EDGE;
                        break;
                    case 33648:
                        state.wrap_s = core::texture_wrap_mode::MIRRORED_REPEAT;
                        break;
                    case 10497:
                        state.wrap_s = core::texture_wrap_mode::REPEAT;
                        break;
                    default:
                        state.wrap_s = core::texture_wrap_mode::REPEAT;
                        break;
                    }
                }

                if ((*sampler)["wrapT"].get(wrap_t))
                {
                    state.wrap_t = core::texture_wrap_mode::REPEAT;
                }
                else
                {
                    switch (wrap_t)
                    {
                    case 33071:
                        state.wrap_t = core::texture_wrap_mode::CLAMP_TO_EDGE;
                        break;
                    case 33648:
                        state.wrap_t = core::texture_wrap_mode::MIRRORED_REPEAT;
                        break;
                    case 10497:
                        state.wrap_t = core::texture_wrap_mode::REPEAT;
                        break;
                    default:
                        state.wrap_t = core::texture_wrap_mode::REPEAT;
                        break;
                    }
                }
            }

            core::texture tex = {
                .sampler = state,
            };

            span<const byte> image_data =
                img.buffer_view_index < 0 ? img.data : buffers.find(img.buffer_view_index)->second;

            // Load image data
            const bool is_16_bit =
                stbi_is_16_bit_from_memory(reinterpret_cast<const stbi_uc*>(image_data.data()), image_data.size());
            int width, height, components;
            stbi_info_from_memory(reinterpret_cast<const stbi_uc*>(image_data.data()), image_data.size(), &width,
                                  &height, &components);

            tex.width = width;
            tex.height = height;

            const bool is_hdr =
                stbi_is_hdr_from_memory(reinterpret_cast<const stbi_uc*>(image_data.data()), image_data.size());

            if (is_hdr)
            {
                tex.format = core::texture_format::RGBA32_FLOAT;
                const auto data = stbi_loadf_from_memory(reinterpret_cast<const stbi_uc*>(image_data.data()),
                                                         image_data.size(), &width, &height, &components, 4);
                auto& mip = tex.mips.emplace_back();
                mip.width = width;
                mip.height = height;
                mip.data.resize(width * height * 4 * sizeof(float));
                copy_n(reinterpret_cast<const byte*>(data), mip.data.size(), mip.data.begin());
                stbi_image_free(data);
            }
            else if (is_16_bit)
            {
                tex.format = core::texture_format::RGBA16_UNORM;
                const auto data = stbi_load_16_from_memory(reinterpret_cast<const stbi_uc*>(image_data.data()),
                                                           image_data.size(), &width, &height, &components, 4);
                auto& mip = tex.mips.emplace_back();
                mip.width = width;
                mip.height = height;
                mip.data.resize(width * height * 4 * sizeof(uint16_t));
                copy_n(reinterpret_cast<const byte*>(data), mip.data.size(), mip.data.begin());
                stbi_image_free(data);
            }
            else
            {
                tex.format = core::texture_format::RGBA8_UNORM;
                const auto data = stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(image_data.data()),
                                                        image_data.size(), &width, &height, &components, 4);
                auto& mip = tex.mips.emplace_back();
                mip.width = width;
                mip.height = height;
                mip.data.resize(width * height * 4);
                copy_n(reinterpret_cast<const byte*>(data), mip.data.size(), mip.data.begin());
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

            return tex_reg->register_texture(tempest::move(tex));
        }

        struct mesh_process_result
        {
            guid mesh_id;
            int32_t material_idx;
        };

        guid process_material(const simdjson::dom::element& mat,
                              const flat_unordered_map<uint32_t, guid>& texture_guids, core::material_registry* mat_reg)
        {
            core::material m;

            std::string_view name;
            if (auto error = mat["name"].get(name); error == simdjson::error_code::SUCCESS)
            {
                m.set_name(name.data());
            }

            sjd::object pbr;
            if (mat["pbrMetallicRoughness"].get(pbr) == simdjson::error_code::SUCCESS)
            {
                sjd::array base_color_factor;
                if (pbr["baseColorFactor"].get(base_color_factor) == simdjson::error_code::SUCCESS)
                {
                    m.set_vec4(core::material::base_color_factor_name,
                               {
                                   static_cast<float>(base_color_factor.at(0).get_double().value()),
                                   static_cast<float>(base_color_factor.at(1).get_double().value()),
                                   static_cast<float>(base_color_factor.at(2).get_double().value()),
                                   static_cast<float>(base_color_factor.at(3).get_double().value()),
                               });
                }

                sjd::object base_color_texture;
                if (pbr["baseColorTexture"].get(base_color_texture) == simdjson::error_code::SUCCESS)
                {
                    m.set_texture(core::material::base_color_texture_name,
                                  texture_guids.find(base_color_texture["index"].get_uint64().value())->second);
                }

                double metallic_factor;
                if (pbr["metallicFactor"].get(metallic_factor) == simdjson::error_code::SUCCESS)
                {
                    m.set_scalar(core::material::metallic_factor_name, static_cast<float>(metallic_factor));
                }
                else
                {
                    m.set_scalar(core::material::metallic_factor_name, 1.0f);
                }

                double roughness_factor;
                if (pbr["roughnessFactor"].get(roughness_factor) == simdjson::error_code::SUCCESS)
                {
                    m.set_scalar(core::material::roughness_factor_name, static_cast<float>(roughness_factor));
                }
                else
                {
                    m.set_scalar(core::material::roughness_factor_name, 1.0f);
                }

                sjd::object metallic_roughness_texture;
                if (pbr["metallicRoughnessTexture"].get(metallic_roughness_texture) == simdjson::error_code::SUCCESS)
                {
                    m.set_texture(core::material::metallic_roughness_texture_name,
                                  texture_guids.find(metallic_roughness_texture["index"].get_uint64().value())->second);
                }
            }

            sjd::object normal_texture;
            if (auto error = mat["normalTexture"].get(normal_texture); error == simdjson::error_code::SUCCESS)
            {
                m.set_texture(core::material::normal_texture_name,
                              texture_guids.find(normal_texture["index"].get_uint64().value())->second);

                double scale;
                if (normal_texture["scale"].get(scale) == simdjson::error_code::SUCCESS)
                {
                    m.set_scalar(core::material::normal_scale_name, static_cast<float>(scale));
                }
                else
                {
                    m.set_scalar(core::material::normal_scale_name, 1.0f);
                }
            }

            sjd::object occlusion_texture;
            if (auto error = mat["occlusionTexture"].get(occlusion_texture); error == simdjson::error_code::SUCCESS)
            {
                m.set_texture(core::material::occlusion_texture_name,
                              texture_guids.find(occlusion_texture["index"].get_uint64().value())->second);

                double strength;
                if (occlusion_texture["strength"].get(strength) == simdjson::error_code::SUCCESS)
                {
                    m.set_scalar(core::material::occlusion_strength_name, static_cast<float>(strength));
                }
                else
                {
                    m.set_scalar(core::material::occlusion_strength_name, 1.0f);
                }
            }

            sjd::object emissive_texture;
            if (auto error = mat["emissiveTexture"].get(emissive_texture); error == simdjson::error_code::SUCCESS)
            {
                m.set_texture(core::material::emissive_texture_name,
                              texture_guids.find(emissive_texture["index"].get_uint64().value())->second);
            }

            sjd::array emissive_factor;
            if (mat["emissiveFactor"].get(emissive_factor) == simdjson::error_code::SUCCESS)
            {
                m.set_vec3(core::material::emissive_factor_name,
                           {
                               static_cast<float>(emissive_factor.at(0).get_double().value()),
                               static_cast<float>(emissive_factor.at(1).get_double().value()),
                               static_cast<float>(emissive_factor.at(2).get_double().value()),
                           });
            }
            else
            {
                m.set_vec3(core::material::emissive_factor_name, {0.0f, 0.0f, 0.0f});
            }

            std::string_view alpha_mode;
            if (mat["alphaMode"].get(alpha_mode) == simdjson::error_code::SUCCESS)
            {
                m.set_string(core::material::alpha_mode_name, alpha_mode.data());
            }
            else
            {
                m.set_string(core::material::alpha_mode_name, "OPAQUE");
            }

            double alpha_cutoff;
            if (mat["alphaCutoff"].get(alpha_cutoff) == simdjson::error_code::SUCCESS)
            {
                m.set_scalar(core::material::alpha_cutoff_name, static_cast<float>(alpha_cutoff));
            }
            else
            {
                m.set_scalar(core::material::alpha_cutoff_name, 0.5f);
            }

            bool double_sided;
            if (mat["doubleSided"].get(double_sided) == simdjson::error_code::SUCCESS)
            {
                m.set_bool(core::material::double_sided_name, double_sided);
            }
            else
            {
                m.set_bool(core::material::double_sided_name, false);
            }

            return mat_reg->register_material(tempest::move(m));
        }

        mesh_process_result process_mesh(const flat_unordered_map<uint32_t, vector<byte>>& buffer_contents,
                                         const simdjson::dom::element& prim, span<buffer_view_payload> views,
                                         span<accessor_payload> accessors, core::mesh_registry* mesh_reg)
        {
            mesh_process_result result;

            core::mesh m;

            sjd::object attribs;
            if (prim["attributes"].get(attribs) == simdjson::error_code::SUCCESS)
            {
                uint64_t positions;
                if (auto positions = attribs["POSITION"].get_uint64();
                    positions.error() == simdjson::error_code::SUCCESS)
                {
                    auto accessor_idx = positions.value();
                    const auto& accessor = accessors[accessor_idx];
                    const auto& view = views[accessor.buffer_view];
                    const vector<byte>& buffer = buffer_contents.find(view.buffer_id)->second;

                    assert(accessor.ctype == component_type::FLOAT);
                    assert(accessor.atype == accessor_type::VEC3);

                    m.vertices.resize(accessor.count);

                    for (uint32_t i = 0; i < accessor.count; ++i)
                    {
                        uint32_t stride_length = view.byte_stride == 0 ? 3 * sizeof(float) : view.byte_stride;
                        uint32_t offset = view.byte_offset + accessor.buffer_offset + i * stride_length;

                        float x = *reinterpret_cast<const float*>(buffer.data() + offset + 0 * sizeof(float));
                        float y = *reinterpret_cast<const float*>(buffer.data() + offset + 1 * sizeof(float));
                        float z = *reinterpret_cast<const float*>(buffer.data() + offset + 2 * sizeof(float));

                        m.vertices[i].position = math::vec3<float>(x, y, z);
                    }
                }

                if (auto normals = attribs["NORMAL"].get_uint64(); normals.error() == simdjson::error_code::SUCCESS)
                {
                    auto accessor_idx = normals.value();
                    const auto& accessor = accessors[accessor_idx];
                    const auto& view = views[accessor.buffer_view];
                    const vector<byte>& buffer = buffer_contents.find(view.buffer_id)->second;

                    assert(accessor.ctype == component_type::FLOAT);
                    assert(accessor.atype == accessor_type::VEC3);

                    for (uint32_t i = 0; i < accessor.count; ++i)
                    {
                        uint32_t stride_length = view.byte_stride == 0 ? 3 * sizeof(float) : view.byte_stride;
                        uint32_t offset = view.byte_offset + accessor.buffer_offset + i * stride_length;

                        float x = *reinterpret_cast<const float*>(buffer.data() + offset + 0 * sizeof(float));
                        float y = *reinterpret_cast<const float*>(buffer.data() + offset + 1 * sizeof(float));
                        float z = *reinterpret_cast<const float*>(buffer.data() + offset + 2 * sizeof(float));

                        m.vertices[i].normal = math::vec3<float>(x, y, z);
                        m.has_normals = true;
                    }
                }

                if (auto uvs = attribs["TEXCOORD_0"].get_uint64(); uvs.error() == simdjson::error_code::SUCCESS)
                {
                    auto accessor_idx = uvs.value();
                    const auto& accessor = accessors[accessor_idx];
                    const auto& view = views[accessor.buffer_view];
                    const vector<byte>& buffer = buffer_contents.find(view.buffer_id)->second;

                    assert(accessor.atype == accessor_type::VEC2);

                    switch (accessor.ctype)
                    {
                    case component_type::FLOAT: {
                        for (uint32_t i = 0; i < accessor.count; ++i)
                        {
                            uint32_t stride_length = view.byte_stride == 0 ? 2 * sizeof(float) : view.byte_stride;
                            uint32_t offset = view.byte_offset + accessor.buffer_offset + i * stride_length;

                            float x = *reinterpret_cast<const float*>(buffer.data() + offset + 0 * sizeof(float));
                            float y = *reinterpret_cast<const float*>(buffer.data() + offset + 1 * sizeof(float));

                            m.vertices[i].uv = math::vec2<float>(x, y);
                        }
                        break;
                    }
                    case component_type::UNSIGNED_SHORT: {
                        assert(accessor.normalized);
                        for (uint32_t i = 0; i < accessor.count; ++i)
                        {
                            uint32_t stride_length = view.byte_stride == 0 ? 2 * sizeof(uint16_t) : view.byte_stride;
                            uint32_t offset = view.byte_offset + accessor.buffer_offset + i * stride_length;

                            uint16_t x =
                                *reinterpret_cast<const uint16_t*>(buffer.data() + offset + 0 * sizeof(uint16_t));
                            uint16_t y =
                                *reinterpret_cast<const uint16_t*>(buffer.data() + offset + 1 * sizeof(uint16_t));

                            m.vertices[i].uv = math::vec2<float>(x / 65535.0f, y / 65535.0f);
                        }
                        break;
                    }
                    case component_type::UNSIGNED_BYTE: {
                        assert(accessor.normalized);
                        for (uint32_t i = 0; i < accessor.count; ++i)
                        {
                            uint32_t stride_length = view.byte_stride == 0 ? 2 * sizeof(uint8_t) : view.byte_stride;
                            uint32_t offset = view.byte_offset + accessor.buffer_offset + i * stride_length;

                            uint8_t x = *reinterpret_cast<const uint8_t*>(buffer.data() + offset + 0 * sizeof(uint8_t));
                            uint8_t y = *reinterpret_cast<const uint8_t*>(buffer.data() + offset + 1 * sizeof(uint8_t));

                            m.vertices[i].uv = math::vec2<float>(x / 255.0f, y / 255.0f);
                        }
                        break;
                    }
                    default:
                        break;
                    }
                }

                if (auto tangents = attribs["TANGENT"].get_uint64(); tangents.error() == simdjson::error_code::SUCCESS)
                {
                    auto accessor_idx = tangents.value();
                    const auto& accessor = accessors[accessor_idx];
                    const auto& view = views[accessor.buffer_view];
                    const vector<byte>& buffer = buffer_contents.find(view.buffer_id)->second;

                    assert(accessor.ctype == component_type::FLOAT);
                    assert(accessor.atype == accessor_type::VEC4);

                    for (uint32_t i = 0; i < accessor.count; ++i)
                    {
                        uint32_t stride_length = view.byte_stride == 0 ? 4 * sizeof(float) : view.byte_stride;
                        uint32_t offset = view.byte_offset + accessor.buffer_offset + i * stride_length;

                        float x = *reinterpret_cast<const float*>(buffer.data() + offset + 0 * sizeof(float));
                        float y = *reinterpret_cast<const float*>(buffer.data() + offset + 1 * sizeof(float));
                        float z = *reinterpret_cast<const float*>(buffer.data() + offset + 2 * sizeof(float));
                        float w = *reinterpret_cast<const float*>(buffer.data() + offset + 3 * sizeof(float));

                        m.vertices[i].tangent = math::vec4<float>(x, y, z, w);
                        m.has_tangents = true;
                    }
                }

                if (auto colors = attribs["COLOR_0"].get_uint64(); colors.error() == simdjson::error_code::SUCCESS)
                {
                    auto accessor_idx = colors.value();
                    const auto& accessor = accessors[accessor_idx];
                    const auto& view = views[accessor.buffer_view];
                    const vector<byte>& buffer = buffer_contents.find(view.buffer_id)->second;

                    assert(accessor.atype == accessor_type::VEC4);

                    switch (accessor.ctype)
                    {
                    case component_type::FLOAT: {
                        for (uint32_t i = 0; i < accessor.count; ++i)
                        {
                            uint32_t stride_length = view.byte_stride == 0 ? 4 * sizeof(float) : view.byte_stride;
                            uint32_t offset = view.byte_offset + accessor.buffer_offset + i * stride_length;

                            float r = *reinterpret_cast<const float*>(buffer.data() + offset + 0 * sizeof(float));
                            float g = *reinterpret_cast<const float*>(buffer.data() + offset + 1 * sizeof(float));
                            float b = *reinterpret_cast<const float*>(buffer.data() + offset + 2 * sizeof(float));
                            float a = *reinterpret_cast<const float*>(buffer.data() + offset + 3 * sizeof(float));

                            m.vertices[i].color = math::vec4<float>(r, g, b, a);
                        }
                        break;
                    }
                    case component_type::UNSIGNED_BYTE: {
                        assert(accessor.normalized);
                        for (uint32_t i = 0; i < accessor.count; ++i)
                        {
                            uint32_t stride_length = view.byte_stride == 0 ? 4 * sizeof(uint8_t) : view.byte_stride;
                            uint32_t offset = view.byte_offset + accessor.buffer_offset + i * stride_length;

                            uint8_t r = *reinterpret_cast<const uint8_t*>(buffer.data() + offset + 0 * sizeof(uint8_t));
                            uint8_t g = *reinterpret_cast<const uint8_t*>(buffer.data() + offset + 1 * sizeof(uint8_t));
                            uint8_t b = *reinterpret_cast<const uint8_t*>(buffer.data() + offset + 2 * sizeof(uint8_t));
                            uint8_t a = *reinterpret_cast<const uint8_t*>(buffer.data() + offset + 3 * sizeof(uint8_t));

                            m.vertices[i].color = math::vec4<float>(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
                        }
                        break;
                    }
                    case component_type::UNSIGNED_SHORT: {
                        assert(accessor.normalized);
                        for (uint32_t i = 0; i < accessor.count; ++i)
                        {
                            uint32_t stride_length = view.byte_stride == 0 ? 4 * sizeof(uint16_t) : view.byte_stride;
                            uint32_t offset = view.byte_offset + accessor.buffer_offset + i * stride_length;

                            uint16_t r =
                                *reinterpret_cast<const uint16_t*>(buffer.data() + offset + 0 * sizeof(uint16_t));
                            uint16_t g =
                                *reinterpret_cast<const uint16_t*>(buffer.data() + offset + 1 * sizeof(uint16_t));
                            uint16_t b =
                                *reinterpret_cast<const uint16_t*>(buffer.data() + offset + 2 * sizeof(uint16_t));
                            uint16_t a =
                                *reinterpret_cast<const uint16_t*>(buffer.data() + offset + 3 * sizeof(uint16_t));

                            m.vertices[i].color =
                                math::vec4<float>(r / 65535.0f, g / 65535.0f, b / 65535.0f, a / 65535.0f);
                        }
                    }
                    default:
                        break;
                    }
                    m.has_colors = true;
                }
            }

            if (auto indices = prim["indices"].get_uint64(); indices.error() == simdjson::error_code::SUCCESS)
            {
                auto accessor_idx = indices.value();
                const auto& accessor = accessors[accessor_idx];
                const auto& view = views[accessor.buffer_view];
                const vector<byte>& buffer = buffer_contents.find(view.buffer_id)->second;

                assert(accessor.atype == accessor_type::SCALAR);

                m.indices.resize(accessor.count);

                switch (accessor.ctype)
                {
                case component_type::UNSIGNED_BYTE: {
                    for (uint32_t i = 0; i < accessor.count; ++i)
                    {
                        uint32_t stride_length = view.byte_stride == 0 ? sizeof(uint8_t) : view.byte_stride;
                        uint32_t offset = view.byte_offset + accessor.buffer_offset + i * stride_length;

                        m.indices[i] = *reinterpret_cast<const uint8_t*>(buffer.data() + offset);
                    }
                    break;
                }
                case component_type::UNSIGNED_SHORT: {
                    for (uint32_t i = 0; i < accessor.count; ++i)
                    {
                        uint32_t stride_length = view.byte_stride == 0 ? sizeof(uint16_t) : view.byte_stride;
                        uint32_t offset = view.byte_offset + accessor.buffer_offset + i * stride_length;

                        m.indices[i] = *reinterpret_cast<const uint16_t*>(buffer.data() + offset);
                    }
                    break;
                }
                case component_type::UNSIGNED_INT: {
                    for (uint32_t i = 0; i < accessor.count; ++i)
                    {
                        uint32_t stride_length = view.byte_stride == 0 ? sizeof(uint32_t) : view.byte_stride;
                        uint32_t offset = view.byte_offset + accessor.buffer_offset + i * stride_length;

                        m.indices[i] = *reinterpret_cast<const uint32_t*>(buffer.data() + offset);
                    }
                    break;
                }
                }
            }

            result.mesh_id = mesh_reg->register_mesh(move(m));

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

    ecs::entity gltf_importer::import(asset_database& db, span<const byte> bytes, ecs::registry& registry,
                                      optional<string_view> path)
    {
        simdjson::dom::parser parser;
        simdjson::padded_string padded(reinterpret_cast<const char*>(bytes.data()), bytes.size());
        auto parse_result = parser.parse(padded);

        if (parse_result.error() != simdjson::error_code::SUCCESS)
        {
            return ecs::null;
        }

        auto ent = registry.acquire_entity();
        auto doc = parse_result.get_object();

        optional<std::filesystem::path> base_path;
        if (path)
        {
            auto p = std::filesystem::path(path->data());
            if (p.has_parent_path())
            {
                base_path = p.parent_path();
            }
        }

        sjd::object asset;
        if (auto error = doc["asset"].get(asset); error == simdjson::SUCCESS)
        {
            asset_database::asset_metadata meta = get_metadata(asset);
            auto meta_id = db.register_asset_metadata(meta);

            asset_metadata_component meta_comp{
                .metadata_id = meta_id,
            };

            registry.assign<asset_metadata_component>(ent, meta_comp);
        }

        flat_unordered_map<uint32_t, vector<byte>> buffer_contents;

        sjd::array buffers;
        if (auto error = doc["buffers"].get(buffers); error == simdjson::SUCCESS)
        {
            uint32_t buffer_id = 0;
            for (const auto& buffer : buffers)
            {
                auto buffer_data = read_buffer(buffer, base_path);
                buffer_contents.insert({buffer_id, move(buffer_data)});
                ++buffer_id;
            }
        }

        flat_unordered_map<uint32_t, image_payload> image_contents;
        sjd::array images;
        if (auto error = doc["images"].get(images); error == simdjson::SUCCESS)
        {
            uint32_t image_id = 0;
            for (const auto& img : images)
            {
                image_payload payload = read_image(img, base_path);
                image_contents.insert({image_id, move(payload)});
                ++image_id;
            }
        }

        auto buffer_views = read_buffer_views(doc.at_key("bufferViews"));
        auto accessors = read_accessors(doc.at_key("accessors"));

        // Process all textures
        flat_unordered_map<uint32_t, guid> texture_guids;
        sjd::array textures;
        if (auto error = doc["textures"].get(textures); error == simdjson::SUCCESS)
        {
            uint32_t texture_id = 0;
            for (const auto& tex : textures)
            {
                auto image_id = tex["source"].get_uint64().value();

                optional<const simdjson::dom::element&> sampler;

                uint64_t sampler_id;
                if (tex["sampler"].get(sampler_id) == simdjson::error_code::SUCCESS)
                {
                    sampler = doc.at_key("samplers").get_array().at(sampler_id).value();
                }

                auto guid =
                    process_texture(image_contents[image_id], sampler, _texture_reg, buffer_views, buffer_contents);
                texture_guids.insert({texture_id, guid});
                ++texture_id;
            }
        }

        // Process all materials
        sjd::array materials;
        flat_unordered_map<int32_t, guid> material_guids;
        if (auto error = doc["materials"].get(materials); error == simdjson::SUCCESS)
        {
            int32_t material_id = 0;
            for (const auto& mat : materials)
            {
                auto guid = process_material(mat, texture_guids, _material_reg);
                material_guids.insert({material_id, guid});

                ++material_id;
            }
        }

        // Mapping of mesh to list of entities representing the primitives
        flat_unordered_map<uint32_t, vector<ecs::entity>> mesh_primitives;
        sjd::array meshes;
        if (auto error = doc["meshes"].get(meshes); error == simdjson::SUCCESS)
        {
            uint32_t mesh_id = 0;
            for (const auto& mesh : meshes)
            {
                vector<ecs::entity> primitives;

                for (const auto& prim : mesh.at("primitives"))
                {
                    auto ent = registry.acquire_entity();
                    auto [mesh_id, material_idx] =
                        process_mesh(buffer_contents, prim, buffer_views, accessors, _mesh_reg);

                    core::mesh_component mesh_comp{
                        .mesh_id = mesh_id,
                    };

                    ecs::transform_component default_tx;
                    registry.assign<core::mesh_component>(ent, mesh_comp);
                    registry.assign<ecs::transform_component>(ent, default_tx);

                    if (material_idx >= 0)
                    {
                        core::material_component mat_comp{
                            .material_id = material_guids.find(material_idx)->second,
                        };

                        registry.assign<core::material_component>(ent, mat_comp);
                    }

                    primitives.push_back(ent);
                }

                // Get mesh name
                std::string_view name;
                if (auto error = mesh["name"].get(name); error == simdjson::SUCCESS)
                {
                    registry.name(ent, name);
                }

                mesh_primitives.insert({mesh_id, move(primitives)});
                ++mesh_id;
            }
        }

        sjd::array nodes;
        if (auto error = doc["nodes"].get(nodes); error == simdjson::SUCCESS)
        {
            // Apply transformations to node, apply child parent relationships to mesh entities and nodes
            flat_unordered_map<uint32_t, ecs::entity> node_entities;
            uint32_t node_id = 0;
            for (const auto& node : nodes)
            {
                auto ent = registry.acquire_entity();

                uint64_t mesh_id;
                if (node["mesh"].get(mesh_id) == simdjson::error_code::SUCCESS)
                {
                    auto mesh_prims = mesh_primitives.find(mesh_id);
                    if (mesh_prims != mesh_primitives.end())
                    {
                        span<const ecs::entity> mesh_entities = mesh_prims->second;
                        for (const auto& mesh_ent : mesh_entities)
                        {
                            ecs::create_parent_child_relationship(registry, ent, mesh_ent);
                        }

                        auto hierarchy_view = ecs::descendant_entity_view(registry, ent);
                    }
                }

                // Get the transform
                ecs::transform_component transform;

                sjd::array translation;
                if (node["translation"].get(translation) == simdjson::error_code::SUCCESS)
                {
                    transform.position({
                        static_cast<float>(translation.at(0).get_double().value()),
                        static_cast<float>(translation.at(1).get_double().value()),
                        static_cast<float>(translation.at(2).get_double().value()),
                    });
                }

                sjd::array rotation;
                if (node["rotation"].get(rotation) == simdjson::error_code::SUCCESS)
                {
                    math::quat<float> quat_rot = {
                        static_cast<float>(rotation.at(0).get_double().value()),
                        static_cast<float>(rotation.at(1).get_double().value()),
                        static_cast<float>(rotation.at(2).get_double().value()),
                        static_cast<float>(rotation.at(3).get_double().value()),
                    };

                    auto m = math::as_mat4(quat_rot);
                    auto te = m.data;

                    const float m11 = te[0], m12 = te[4], m13 = te[8];
                    const float m21 = te[1], m22 = te[5], m23 = te[9];
                    const float m31 = te[2], m32 = te[6], m33 = te[10];

                    float x, z;
                    auto y = std::asin(math::clamp(m13, -1.0f, 1.0f));

                    if (std::abs(m13) < 0.9999999)
                    {

                        x = std::atan2(-m23, m33);
                        z = std::atan2(-m12, m11);
                    }
                    else
                    {

                        x = std::atan2(m32, m22);
                        z = 0;
                    }

                    transform.rotation({x, y, z});
                }

                sjd::array scale;
                if (node["scale"].get(scale) == simdjson::error_code::SUCCESS)
                {
                    transform.scale({
                        static_cast<float>(scale.at(0).get_double().value()),
                        static_cast<float>(scale.at(1).get_double().value()),
                        static_cast<float>(scale.at(2).get_double().value()),
                    });
                }

                sjd::array matrix;
                if (node["matrix"].get(matrix) == simdjson::error_code::SUCCESS)
                {
                    // TODO: Support matrix extration
                }

                registry.assign<ecs::transform_component>(ent, transform);

                node_entities.insert({node_id, ent});
                ++node_id;
            }

            // Apply parent child relationships
            node_id = 0;
            for (const auto& node : nodes)
            {
                auto node_ent = node_entities.find(node_id)->second;

                sjd::array children;
                if (node["children"].get(children) == simdjson::error_code::SUCCESS)
                {
                    for (const auto& child : children)
                    {
                        uint32_t child_id = child.get_uint64().value();
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
                auto rel = registry.try_get<ecs::relationship_component<ecs::entity>>(e);
                if (rel == nullptr || rel->parent == ecs::tombstone)
                {
                    ecs::create_parent_child_relationship(registry, ent, e);
                }
            }
        }

        auto ent_hierarchy_view = ecs::descendant_entity_view(registry, ent);
        auto ent_in_view_count = tempest::distance(ent_hierarchy_view.begin(), ent_hierarchy_view.end());

        return ent;
    }
} // namespace tempest::assets