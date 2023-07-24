#ifndef tempest_assets_material_asset_hpp
#define tempest_assets_material_asset_hpp

#include <tempest/assets.hpp>
#include <tempest/assets/asset.hpp>

#include <string>
#include <string_view>

namespace tempest::assets
{
    class material_asset : public asset
    {
      public:
        material_asset(std::string_view name) : asset(name)
        {
        }
    };
} // namespace tempest::assets

#endif // tempest_assets_material_asset_hpp