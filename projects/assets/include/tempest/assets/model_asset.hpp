#ifndef tempest_assets_model_asset_hpp
#define tempest_assets_model_asset_hpp

#include <tempest/assets.hpp>
#include <tempest/assets/asset.hpp>

#include <string>
#include <string_view>

namespace tempest::assets
{
    class model_asset : public asset
    {
      public:
        model_asset(std::string_view name) : asset(name)
        {
        }
    };
} // namespace tempest::assets

#endif // tempest_assets_model_asset_hpp