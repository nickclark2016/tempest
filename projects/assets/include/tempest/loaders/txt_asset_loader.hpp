#ifndef tempest_assets_txt_asset_loader_hpp
#define tempest_assets_txt_asset_loader_hpp

#include <tempest/assets.hpp>
#include <tempest/assets/txt_asset.hpp>

#include <filesystem>
#include <memory>

namespace tempest::assets
{
    class txt_asset_loader : public asset_loader
    {
      public:
        bool load(const std::filesystem::path& path, void* dest) override;
        bool release(std::unique_ptr<asset>&& asset) override;

        using type = txt_asset;
    };
} // namespace tempest::assets

#endif // tempest_assets_txt_asset_loader_hpp