#include <tempest/loaders/model_asset_loader.hpp>

#include <tempest/assets/mesh_asset.hpp>
#include <tempest/assets/material_asset.hpp>

#include <fstream>
#include <streambuf>
#include <string>

namespace tempest::assets
{
    model_asset_loader::model_asset_loader(asset_pool* mesh_pool, asset_pool* material_pool)
    {
        _mesh_asset_pool = mesh_pool;
        _material_asset_pool = material_pool;
    }

    bool model_asset_loader::load(const std::filesystem::path& path, void* dest)
    {
        return true;
    }

    bool model_asset_loader::release(std::unique_ptr<asset>&& asset)
    {
        return false;
    }
} // namespace tempest::assets