#ifndef tempest_assets_txt_asset_hpp
#define tempest_assets_txt_asset_hpp

#include <tempest/assets.hpp>
#include <tempest/assets/asset.hpp>

#include <string>
#include <string_view>

namespace tempest::assets
{
    class txt_asset : public asset
    {
      public:
        txt_asset(std::string_view name) : asset(name)
        {
        }
        std::string data;
    };
} // namespace tempest::assets

#endif // tempest_assets_txt_asset_hpp