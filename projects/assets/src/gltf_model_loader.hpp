#ifndef tempest_gltfmodel_hpp
#define tempest_gltfmodel_hpp

#include <tempest/assets/model_asset.hpp>
#include <tempest/vertex.hpp>

#include <tinygltf/tiny_gltf.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace tempest::assets
{
    class gltf_model_loader
    {
      public:
        static bool load(const std::filesystem::path& path, void* dest, asset_pool* mesh_pool,
                         asset_pool* material_pool, core::heap_allocator* vertex_data_alloc);

      private:
        static void load_node(const tinygltf::Node& input_node, const tinygltf::Model& input, model_node* parent,
                              std::vector<std::uint32_t>& index_buffer, std::vector<core::vertex>& vertex_buffer,
                              asset_pool* mesh_pool, asset_pool* material_pool,
                              core::heap_allocator* vertex_data_alloc);
    };
} // namespace tempest::assets

#endif // tempest_gltfmodel_hpp