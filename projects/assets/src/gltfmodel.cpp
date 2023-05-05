#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define __STDC_LIB_EXT1__

#include "gltfmodel.hpp"
#include <span>
#include <tempest/logger.hpp>

namespace tempest::assets::gltf
{
    void gltfmodel::load_node(const tinygltf::Node& input_node, const tinygltf::Model& input, gltfmodel::node* parent,
                              std::vector<std::uint32_t>& index_buffer, std::vector<core::vertex>& vertex_buffer)
    {
        gltfmodel::node* node = new gltfmodel::node();
        node->name = input_node.name;
        node->parent = parent;

        node->matrix = tempest::math::mat<float, 4, 4>(1.0f);
        if (input_node.translation.size() == 3)
        {
            // assign translation matrix
            // node->matrix =
        }

        if (input_node.rotation.size() == 4)
        {
            // multiple matrix by rotation
        }

        if (input_node.scale.size() == 3)
        {
            // assign scale matrix
        }

        if (input_node.matrix.size() == 16)
        {
            // direct assign matrix
        }

        // Load node children
        if (input_node.children.size() > 0)
        {
            for (std::size_t i = 0; i < input_node.children.size(); i++)
            {
                load_node(input.nodes[input_node.children[i]], input, node, index_buffer, vertex_buffer);
            }
        }

        // If node contains mesh data
        if (input_node.mesh > -1)
        {
            node->m = new mesh();
            const tinygltf::Mesh mesh = input.meshes[input_node.mesh];
            for (std::size_t i = 0; i < mesh.primitives.size(); i++)
            {
                const tinygltf::Primitive& gltf_primitive = mesh.primitives[i];
                std::uint32_t first_index = static_cast<std::uint32_t>(index_buffer.size());
                std::uint32_t vertex_start = static_cast<std::uint32_t>(vertex_buffer.size());
                std::uint32_t index_count = 0;

                // Vertices
                {
                    const float* position_buffer = nullptr;
                    const float* normals_buffer = nullptr;
                    const float* texcoords_buffer = nullptr;
                    const float* tangents_buffer = nullptr;
                    std::uint32_t vertex_count = 0;

                    if (gltf_primitive.attributes.find("POSITION") != gltf_primitive.attributes.end())
                    {
                        const tinygltf::Accessor& accessor =
                            input.accessors[gltf_primitive.attributes.find("POSITION")->second];

                        const tinygltf::BufferView& view = input.bufferViews[accessor.bufferView];
                        position_buffer = reinterpret_cast<const float*>(
                            &(input.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
                        vertex_count = static_cast<std::uint32_t>(accessor.count);
                    }

                    if (gltf_primitive.attributes.find("NORMAL") != gltf_primitive.attributes.end())
                    {
                        const tinygltf::Accessor& accessor =
                            input.accessors[gltf_primitive.attributes.find("NORMAL")->second];

                        const tinygltf::BufferView& view = input.bufferViews[accessor.bufferView];
                        normals_buffer = reinterpret_cast<const float*>(
                            &(input.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
                    }

                    if (gltf_primitive.attributes.find("TEXCOORD_0") != gltf_primitive.attributes.end())
                    {
                        const tinygltf::Accessor& accessor =
                            input.accessors[gltf_primitive.attributes.find("TEXCOORD_0")->second];

                        const tinygltf::BufferView& view = input.bufferViews[accessor.bufferView];
                        texcoords_buffer = reinterpret_cast<const float*>(
                            &(input.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
                    }

                    if (gltf_primitive.attributes.find("TANGENT") != gltf_primitive.attributes.end())
                    {
                        const tinygltf::Accessor& accessor =
                            input.accessors[gltf_primitive.attributes.find("TANGENT")->second];

                        const tinygltf::BufferView& view = input.bufferViews[accessor.bufferView];
                        tangents_buffer = reinterpret_cast<const float*>(
                            &(input.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
                    }

                    for (std::size_t i = 0; i < vertex_count; i++)
                    {
                        core::vertex vert{};
                        vert.position.set(position_buffer[i * 3], position_buffer[i * 3 + 1],
                                          position_buffer[i * 3 + 2]);

                        if (normals_buffer)
                        {
                            vert.normal.set(normals_buffer[i * 2], normals_buffer[i * 2 + 1],
                                            normals_buffer[i * 2 + 2]);
                        }
                        else
                            vert.normal.zero();

                        if (texcoords_buffer)
                        {
                            vert.uv.set(texcoords_buffer[i * 2], texcoords_buffer[i * 2 + 1]);
                        }
                        else
                            vert.uv.zero();

                        if (tangents_buffer)
                        {
                            vert.tangent.set(tangents_buffer[i * 4], tangents_buffer[i * 4 + 1],
                                             tangents_buffer[i * 4 + 2], tangents_buffer[i * 4 + 3]);
                        }
                        else
                            vert.tangent.zero();

                        vertex_buffer.push_back(vert);
                    }
                }

                // Indices
                {
                    const tinygltf::Accessor& accessor = input.accessors[gltf_primitive.indices];
                    const tinygltf::BufferView& buffer_view = input.bufferViews[accessor.bufferView];
                    const tinygltf::Buffer& buffer = input.buffers[buffer_view.buffer];

                    index_count += static_cast<std::uint32_t>(accessor.count);

                    switch (accessor.componentType)
                    {
                    case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT: {
                        const std::uint32_t* buf = reinterpret_cast<const std::uint32_t*>(
                            &buffer.data[accessor.byteOffset + buffer_view.byteOffset]);
                        for (std::size_t index = 0; index < accessor.count; index++)
                        {
                            index_buffer.push_back(buf[index] + vertex_start);
                        }
                        break;
                    }
                    case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT: {
                        const std::uint16_t* buf = reinterpret_cast<const std::uint16_t*>(
                            &buffer.data[accessor.byteOffset + buffer_view.byteOffset]);
                        for (std::size_t index = 0; index < accessor.count; index++)
                        {
                            index_buffer.push_back(buf[index] + vertex_start);
                        }
                        break;
                    }
                    case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE: {
                        const std::uint8_t* buf = reinterpret_cast<const std::uint8_t*>(
                            &buffer.data[accessor.byteOffset + buffer_view.byteOffset]);
                        for (std::size_t index = 0; index < accessor.count; index++)
                        {
                            index_buffer.push_back(buf[index] + vertex_start);
                        }
                        break;
                    }
                    }
                }

                gltfmodel::primitive primitive{};
                primitive.first_index = first_index;
                primitive.index_count = index_count;
                primitive.material_index = gltf_primitive.material;
                node->m->primitives.push_back(primitive);
            }
        }

        if (parent)
        {
            parent->children.push_back(node);
        }
        else
        {
            nodes.push_back(node);
        }
    }

    bool gltfmodel::load_from_binary(const std::vector<unsigned char>& binary_data)
    {
        std::string err;
        std::string warn;

        auto logger = tempest::logger::logger_factory::create({
            .prefix{"Tempest::Assets"},
        });

        bool ret = _loader.LoadBinaryFromMemory(&_model, &err, &warn, binary_data.data(),
                                                static_cast<std::uint32_t>(binary_data.size()), "data");

        if (!warn.empty())
        {
            logger->warn(warn);
        }

        if (!err.empty())
        {
            logger->error(err);
        }

        if (!ret)
        {
            return false;
        }

        //std::vector<std::uint32_t> index_buffer;
        //std::vector<vertex> vertex_buffer;

        const tinygltf::Scene& scene = _model.scenes[0];
        for (std::size_t i = 0; i < scene.nodes.size(); i++)
        {
            const tinygltf::Node node = _model.nodes[scene.nodes[i]];
            load_node(node, _model, nullptr, indices, vertices);
        }

        return true;
    }

    bool gltfmodel::load_from_ascii(const std::string& ascii_data)
    {
        std::string err;
        std::string warn;

        auto logger = tempest::logger::logger_factory::create({
            .prefix{"Tempest::Assets"},
        });

        bool ret = _loader.LoadASCIIFromString(&_model, &err, &warn, ascii_data.c_str(),
                                               static_cast<std::uint32_t>(ascii_data.length()), "data");

        if (!warn.empty())
        {
            logger->warn(warn);
        }

        if (!err.empty())
        {
            logger->error(err);
        }

        if (!ret)
        {
            return false;
        }

        //std::vector<std::uint32_t> index_buffer;
        //std::vector<vertex> vertex_buffer;

        const tinygltf::Scene& scene = _model.scenes[0];
        for (std::size_t i = 0; i < scene.nodes.size(); i++)
        {
            const tinygltf::Node node = _model.nodes[scene.nodes[i]];
            load_node(node, _model, nullptr, indices, vertices);
        }

        return true;
    }
} // namespace tempest::assets::gltf
