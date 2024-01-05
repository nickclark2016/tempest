#include <tempest/mesh_asset.hpp>

#include <tempest/math_utils.hpp>
#include <tempest/quat.hpp>
#include <tempest/transformations.hpp>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tinygltf/tiny_gltf.h>

#include <string_view>

#undef OPAQUE

namespace tempest::assets
{
    namespace
    {
        std::size_t element_size(int e, int t)
        {
            std::uint32_t el_size = ([&]() {
                switch (e)
                {
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                    return 1;
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                    return 2;
                case TINYGLTF_COMPONENT_TYPE_FLOAT:
                    [[fallthrough]];
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                    return 4;
                }
                return 0;
            })();

            std::uint32_t el_count = ([&]() {
                switch (t)
                {
                case TINYGLTF_TYPE_SCALAR:
                    return 1;
                case TINYGLTF_TYPE_VEC2:
                    return 2;
                case TINYGLTF_TYPE_VEC3:
                    return 3;
                case TINYGLTF_TYPE_VEC4:
                    return 4;
                case TINYGLTF_TYPE_MAT2:
                    return 4;
                case TINYGLTF_TYPE_MAT3:
                    return 9;
                case TINYGLTF_TYPE_MAT4:
                    return 16;
                }
                return 0;
            })();

            return el_size * el_count;
        }

        unsigned int read_index_from_ubyte(const unsigned char* data)
        {
            return *reinterpret_cast<const unsigned char*>(data);
        }

        unsigned int read_index_from_ushort(const unsigned char* data)
        {
            return *reinterpret_cast<const unsigned short*>(data);
        }

        unsigned int read_index_from_uint(const unsigned char* data)
        {
            return *reinterpret_cast<const unsigned int*>(data);
        }

        float read_float(const unsigned char* data)
        {
            return *reinterpret_cast<const float*>(data);
        }

        math::vec2<float> read_float_2(const unsigned char* data)
        {
            return {read_float(data), read_float(data + sizeof(float))};
        }

        math::vec3<float> read_float_3(const unsigned char* data)
        {
            return {read_float(data), read_float(data + sizeof(float)), read_float(data + 2 * sizeof(float))};
        }

        math::vec4<float> read_float_4(const unsigned char* data)
        {
            return {read_float(data), read_float(data + sizeof(float)), read_float(data + 2 * sizeof(float)),
                    read_float(data + 3 * sizeof(float))};
        }

        material_type get_material_type(std::string_view type)
        {
            if (type == "OPAQUE")
            {
                return material_type::OPAQUE;
            }
            else if (type == "MASK")
            {
                return material_type::MASK;
            }
            else if (type == "BLEND")
            {
                return material_type::BLEND;
            }

            return material_type::OPAQUE;
        }
    } // namespace

    std::optional<scene_asset> load_scene(const std::filesystem::path& path)
    {
        tinygltf::Model root;
        tinygltf::TinyGLTF loader;

        std::string err, warn;

        bool ret = loader.LoadASCIIFromFile(&root, &err, &warn, path.string().c_str());
        if (!ret)
        {
            return std::nullopt;
        }

        scene_asset asset;

        std::unordered_map<std::size_t, std::vector<std::size_t>> mesh_to_prims;

        std::size_t mesh_id = 0;
        std::size_t prim_id = 0;

        for (const auto& mesh : root.meshes)
        {
            for (const auto& prim : mesh.primitives)
            {
                mesh_asset m;

                auto positions_accessor_it = prim.attributes.find("POSITION");
                auto uvs_accessor_it = prim.attributes.find("TEXCOORD_0");
                auto normals_accessor_it = prim.attributes.find("NORMAL");
                auto tangents_accessor_it = prim.attributes.find("TANGENT");

                const auto& indices_accessor = root.accessors[prim.indices];
                const auto& index_buffer_view = root.bufferViews[indices_accessor.bufferView];
                const auto& index_buffer = root.buffers[index_buffer_view.buffer];

                m.mesh.indices.resize(indices_accessor.count);

                auto el_size = element_size(indices_accessor.componentType, indices_accessor.type);
                auto read_fn = ([&indices_accessor]() {
                    switch (indices_accessor.componentType)
                    {
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                        return read_index_from_ubyte;
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                        return read_index_from_ushort;
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                        return read_index_from_uint;
                    default:
                        std::exit(EXIT_FAILURE);
                    }
                })();

                auto stride = index_buffer_view.byteStride == 0 ? el_size : index_buffer_view.byteStride;

                for (std::size_t i = 0; i < indices_accessor.count; ++i)
                {
                    auto read_offset = indices_accessor.byteOffset + i * stride + index_buffer_view.byteOffset;
                    m.mesh.indices[i] = read_fn(index_buffer.data.data() + read_offset);
                }

                m.material_id = prim.material;
                m.mesh.has_normals = normals_accessor_it != prim.attributes.end();
                m.mesh.has_tangents = tangents_accessor_it != prim.attributes.end();
                m.mesh.has_colors = false;

                const auto& positions_accessor = root.accessors[positions_accessor_it->second];
                const auto& positions_buffer_view = root.bufferViews[positions_accessor.bufferView];
                const auto& positions_buffer = root.buffers[positions_buffer_view.buffer];

                m.mesh.vertices.resize(positions_accessor.count);

                el_size = element_size(positions_accessor.componentType, positions_accessor.type);
                stride = positions_buffer_view.byteStride == 0 ? el_size : positions_buffer_view.byteStride;

                for (std::size_t i = 0; i < positions_accessor.count; ++i)
                {
                    auto read_offset = positions_accessor.byteOffset + i * stride + positions_buffer_view.byteOffset;
                    m.mesh.vertices[i].position = read_float_3(positions_buffer.data.data() + read_offset);
                }

                const auto& uvs_accessor = root.accessors[uvs_accessor_it->second];
                const auto& uvs_buffer_view = root.bufferViews[uvs_accessor.bufferView];
                const auto& uvs_buffer = root.buffers[uvs_buffer_view.buffer];

                el_size = element_size(uvs_accessor.componentType, uvs_accessor.type);
                stride = uvs_buffer_view.byteStride == 0 ? el_size : uvs_buffer_view.byteStride;

                for (std::size_t i = 0; i < uvs_accessor.count; ++i)
                {
                    auto read_offset = uvs_accessor.byteOffset + i * stride + uvs_buffer_view.byteOffset;
                    m.mesh.vertices[i].uv = read_float_2(uvs_buffer.data.data() + read_offset);
                }

                if (m.mesh.has_normals)
                {
                    const auto& normals_accessor = root.accessors[normals_accessor_it->second];
                    const auto& normals_buffer_view = root.bufferViews[normals_accessor.bufferView];
                    const auto& normals_buffer = root.buffers[normals_buffer_view.buffer];

                    el_size = element_size(normals_accessor.componentType, normals_accessor.type);
                    stride = normals_buffer_view.byteStride == 0 ? el_size : normals_buffer_view.byteStride;

                    for (std::size_t i = 0; i < normals_accessor.count; ++i)
                    {
                        auto read_offset = normals_accessor.byteOffset + i * stride + normals_buffer_view.byteOffset;
                        m.mesh.vertices[i].normal = read_float_3(normals_buffer.data.data() + read_offset);
                    }
                }

                if (m.mesh.has_tangents)
                {
                    const auto& tangents_accessor = root.accessors[tangents_accessor_it->second];
                    const auto& tangents_buffer_view = root.bufferViews[tangents_accessor.bufferView];
                    const auto& tangents_buffer = root.buffers[tangents_buffer_view.buffer];

                    el_size = element_size(tangents_accessor.componentType, tangents_accessor.type);
                    stride = tangents_buffer_view.byteStride == 0 ? el_size : tangents_buffer_view.byteStride;

                    for (std::size_t i = 0; i < tangents_accessor.count; ++i)
                    {
                        auto read_offset = tangents_accessor.byteOffset + i * stride + tangents_buffer_view.byteOffset;
                        m.mesh.vertices[i].tangent = read_float_4(tangents_buffer.data.data() + read_offset);
                    }
                }

                m.mesh.name = mesh.name;

                asset.meshes.push_back(m);

                mesh_to_prims[mesh_id].push_back(prim_id++);
            }

            mesh_id++;
        }

        std::unordered_map<std::size_t, std::size_t> node_internal_to_gltf;
        std::unordered_map<std::size_t, std::size_t> node_gltf_to_internal;

        std::uint32_t node_id = 0;
        for (auto& node : root.nodes)
        {
            scene_asset_node n{
                .name = node.name,
            };

            if (!node.translation.empty())
            {
                n.position = {
                    static_cast<float>(node.translation[0]),
                    static_cast<float>(node.translation[1]),
                    static_cast<float>(node.translation[2]),
                };
            }

            if (node.rotation.size() == 3)
            {
                n.rotation = {
                    static_cast<float>(node.rotation[0]),
                    static_cast<float>(node.rotation[1]),
                    static_cast<float>(node.rotation[2]),
                };
            }
            else if (node.rotation.size() == 4)
            {
                math::quat<float> quat_rot = {
                    static_cast<float>(node.rotation[1]),
                    static_cast<float>(node.rotation[2]),
                    static_cast<float>(node.rotation[3]),
                    static_cast<float>(node.rotation[0]),
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

                n.rotation = {x, y, z};
            }

            if (!node.scale.empty())
            {
                n.scale = {
                    static_cast<float>(node.scale[0]),
                    static_cast<float>(node.scale[1]),
                    static_cast<float>(node.scale[2]),
                };
            }
            else
            {
                n.scale = {1.0f, 1.0f, 1.0f};
            }

            n.children.reserve(node.children.size());
            for (int child : node.children)
            {
                n.children.push_back(static_cast<std::uint32_t>(child));
            }

            for (auto& prim_id : mesh_to_prims[node.mesh])
            {
                n.mesh_id = static_cast<std::uint32_t>(prim_id);
                asset.nodes.push_back(n);

                node_internal_to_gltf[asset.nodes.size() - 1] = node_id;
                node_gltf_to_internal[node_id] = asset.nodes.size() - 1;
            }

            ++node_id;
        }

        node_id = 0;
        for (auto& node : asset.nodes)
        {
            for (auto& child : node.children)
            {
                child = static_cast<std::uint32_t>(node_gltf_to_internal[child]);
                asset.nodes[child].parent = node_id;
            }
            ++node_id;
        }

        for (auto& material : root.materials)
        {
            material_asset mat{
                .name = material.name,
            };

            if (material.pbrMetallicRoughness.baseColorTexture.index != -1)
            {
                mat.base_color_texture = static_cast<std::uint32_t>(
                    root.textures[material.pbrMetallicRoughness.baseColorTexture.index].source);
            }

            if (material.normalTexture.index != -1)
            {
                mat.normal_map_texture = static_cast<std::uint32_t>(root.textures[material.normalTexture.index].source);
            }

            if (material.pbrMetallicRoughness.metallicRoughnessTexture.index != -1)
            {
                mat.metallic_roughness_texture = static_cast<std::uint32_t>(
                    root.textures[material.pbrMetallicRoughness.metallicRoughnessTexture.index].source);
            }

            if (material.occlusionTexture.index != -1)
            {
                mat.occlusion_map_texture =
                    static_cast<std::uint32_t>(root.textures[material.occlusionTexture.index].source);
            }

            if (material.emissiveTexture.index != -1)
            {
                mat.emissive_map_texture =
                    static_cast<std::uint32_t>(root.textures[material.emissiveTexture.index].source);
            }

            mat.type = get_material_type(material.alphaMode);

            asset.materials.push_back(mat);
        }

        auto root_path = path.parent_path();

        for (auto& image : root.images)
        {
            texture_asset tex = {
                .width = static_cast<std::uint32_t>(image.width),
                .height = static_cast<std::uint32_t>(image.height),
                .bit_depth = static_cast<std::uint32_t>(image.bits),
            };

            tex.data.resize(image.image.size());
            std::memcpy(tex.data.data(), image.image.data(), image.image.size());

            asset.textures.push_back(std::move(tex));
        }

        return asset;
    }
} // namespace tempest::assets