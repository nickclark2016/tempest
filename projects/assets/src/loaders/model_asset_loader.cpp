#include <tempest/loaders/model_asset_loader.hpp>

#include <tempest/assets/material_asset.hpp>
#include <tempest/assets/mesh_asset.hpp>

#include "../gltf_model_loader.hpp"

#include <fstream>
#include <streambuf>
#include <string>

namespace tempest::assets
{
    namespace detail
    {

    }

    model_asset_loader::model_asset_loader(asset_pool* mesh_pool, asset_pool* material_pool,
                                           core::heap_allocator* vertex_data_alloc)
    {
        _mesh_asset_pool = mesh_pool;
        _material_asset_pool = material_pool;
        _vertex_data_alloc = vertex_data_alloc;
    }

    bool model_asset_loader::load(const std::filesystem::path& path, void* dest)
    {
        auto logger = tempest::logger::logger_factory::create({
            .prefix{"tempest::assets"},
        });

        // Force lower case for easy comparison
        std::string extension = path.extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(),
                       [](unsigned char c) { return std::tolower(c); });

        if (extension == ".glb" || extension == ".gltf")
        {
            // GLTF
            gltf_model_loader::load(path, dest, _mesh_asset_pool, _material_asset_pool, _vertex_data_alloc);
        }
        else if (extension == ".fbx")
        {
            // FBX
        }

        return true;
    }

    bool model_asset_loader::release(std::unique_ptr<asset>&& asset)
    {
        return false;
    }
} // namespace tempest::assets