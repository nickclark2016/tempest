#ifndef tempest_assets_mesh_asset_hpp
#define tempest_assets_mesh_asset_hpp

#include <tempest/assets.hpp>
#include <tempest/assets/asset.hpp>

#include <string>
#include <string_view>

namespace tempest::assets
{
    class mesh_asset : public asset
    {
      public:
        mesh_asset(std::string_view name) : asset(name)
        {
        }
    };
} // namespace tempest::assets

#endif // tempest_assets_mesh_asset_hpp