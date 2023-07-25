#include <tempest/assets.hpp>

#include <tempest/loaders/model_asset_loader.hpp>

#include <tempest/assets/material_asset.hpp>
#include <tempest/assets/mesh_asset.hpp>

namespace tempest::assets
{
    std::size_t counter::value = 0;

    asset_manager::asset_manager()
        : _alloc{128 * 1024 * 1024}
    {
        /// TODO: Find a way to properly determine these sizes
        auto mesh_asset_pool =
            _asset_pools.emplace(std::piecewise_construct, std::forward_as_tuple(typeid_counter<mesh_asset>::value()),
                                 std::forward_as_tuple(&_alloc, 64, static_cast<std::uint32_t>(sizeof(mesh_asset))));
        auto material_asset_pool = _asset_pools.emplace(
            std::piecewise_construct, std::forward_as_tuple(typeid_counter<material_asset>::value()),
            std::forward_as_tuple(&_alloc, 64, static_cast<std::uint32_t>(sizeof(material_asset))));

        register_loader<model_asset_loader>(&mesh_asset_pool.first->second, &material_asset_pool.first->second, &_alloc);
    }

    asset_manager::~asset_manager()
    {
        _asset_pools.clear();
    }
} // namespace tempest::assets