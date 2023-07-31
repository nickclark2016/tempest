#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define __STDC_LIB_EXT1__

#include "gltf_model_loader.hpp"
#include <span>
#include <tempest/logger.hpp>

#include <tempest/assets/mesh_asset.hpp>
#include <tempest/transformations.hpp>

#include <array>

namespace tempest::assets
{
    void gltf_model_loader::load_node(const tinygltf::Node& input_node, const tinygltf::Model& input,
                                      model_node* parent, std::vector<std::uint32_t>& index_buffer,
                                      std::vector<core::vertex>& vertex_buffer, asset_pool* mesh_pool,
                                      asset_pool* material_pool, core::heap_allocator* vertex_data_alloc)
    {
        model_node* node = new model_node();
        node->name = input_node.name;
        node->parent = parent;

        node->matrix = tempest::math::mat4<float>(1.0f);
        if (input_node.translation.size() == 3)
        {
            // assign translation matrix
            /* math::translate(node->matrix, {static_cast<float>(input_node.translation[0]),
                                           static_cast<float>(input_node.translation[1]),
                                           static_cast<float>(input_node.translation[2])});*/
        }

        if (input_node.rotation.size() == 4)
        {
            // multiple matrix by rotation
        }

        if (input_node.scale.size() == 3)
        {
            // assign scale matrix
            /* math::scale(node->matrix,
                             {static_cast<float>(input_node.scale[0]), static_cast<float>(input_node.scale[1]),
                                       static_cast<float>(input_node.scale[2])});*/
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
                load_node(input.nodes[input_node.children[i]], input, node, index_buffer, vertex_buffer, mesh_pool,
                          material_pool, vertex_data_alloc);
            }
        }

        // If node contains mesh data
        if (input_node.mesh > -1)
        {
            auto pool_id = mesh_pool->object_pool.acquire_resource();
            auto pool_pointer = mesh_pool->object_pool.access(pool_id);

            mesh_asset* m = std::construct_at(reinterpret_cast<mesh_asset*>(pool_pointer),
                                              input_node.name + "_" + std::to_string(input_node.mesh));

            node->mesh = m;

            const tinygltf::Mesh mesh = input.meshes[input_node.mesh];
            std::vector<mesh_primitive> primitives;
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
                        vert.position = {position_buffer[i * 3], position_buffer[i * 3 + 1],
                                         position_buffer[i * 3 + 2]};

                        if (normals_buffer)
                        {
                            vert.normal = {normals_buffer[i * 2], normals_buffer[i * 2 + 1], normals_buffer[i * 2 + 2]};
                        }

                        if (texcoords_buffer)
                        {
                            vert.uv = {texcoords_buffer[i * 2], texcoords_buffer[i * 2 + 1]};
                        }

                        if (tangents_buffer)
                        {
                            vert.tangent = {tangents_buffer[i * 4], tangents_buffer[i * 4 + 1],
                                            tangents_buffer[i * 4 + 2], tangents_buffer[i * 4 + 3]};
                        }

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

                mesh_primitive primitive{
                    .first_index{first_index}, .index_count{index_count}, .material_index{gltf_primitive.material}};

                primitives.push_back(primitive);
            }

            node->mesh->primitive_count = static_cast<std::uint32_t>(primitives.size());
            node->mesh->primitives = reinterpret_cast<mesh_primitive*>(vertex_data_alloc->allocate(primitives.size() * sizeof(mesh_primitive), 1));

            for (std::size_t i = 0; i < primitives.size(); i++)
            {
                node->mesh->primitives[i] = primitives[i];
            }
        }

        if (parent)
        {
            parent->children.emplace_back(node);
        }
        else
        {
            // root = node;
        }
    }

    bool gltf_model_loader::load(const std::filesystem::path& path, void* dest, asset_pool* mesh_pool,
                                 asset_pool* material_pool, core::heap_allocator* vertex_data_alloc)
    {
        std::string err;
        std::string warn;

        auto logger = tempest::logger::logger_factory::create({
            .prefix{"tempest::assets::gltf"},
        });

        tinygltf::TinyGLTF loader;
        tinygltf::Model model;

        std::string extension = path.extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(),
                       [](unsigned char c) { return std::tolower(c); });

        bool ret = false;

        if (extension == ".glb")
        {
            ret = loader.LoadBinaryFromFile(&model, &err, &warn, path.string());
        }
        else if (extension == ".gltf")
        {
            ret = loader.LoadASCIIFromFile(&model, &err, &warn, path.string());
        }

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

        model_asset* asset = std::construct_at(reinterpret_cast<model_asset*>(dest), path.string());

        std::vector<std::uint32_t> indices;
        std::vector<core::vertex> vertices;

        asset->root = new model_node{.name{path.string()}, .parent{nullptr}};

        const tinygltf::Scene& scene = model.scenes[0];
        for (std::size_t i = 0; i < scene.nodes.size(); i++)
        {
            const tinygltf::Node node = model.nodes[scene.nodes[i]];
            load_node(node, model, asset->root, indices, vertices, mesh_pool, material_pool, vertex_data_alloc);
        }

        asset->index_count = static_cast<std::uint32_t>(indices.size());
        asset->vertex_count = static_cast<std::uint32_t>(vertices.size());

        asset->vertices =
            reinterpret_cast<core::vertex*>(vertex_data_alloc->allocate(sizeof(core::vertex) * vertices.size(), 1));
        asset->indices =
            reinterpret_cast<std::uint32_t*>(vertex_data_alloc->allocate(sizeof(std::uint32_t) * indices.size(), 1));

        for (std::size_t i = 0; i < vertices.size(); i++)
        {
            asset->vertices[i] = vertices[i];
        }
        for (std::size_t i = 0; i < indices.size(); i++)
        {
            asset->indices[i] = indices[i];
        }

        return true;
    }
} // namespace tempest::assets
