#ifndef tempest_assets_asset_loader_hpp
#define tempest_assets_asset_loader_hpp

#include <tempest/assets/asset.hpp>

#include <filesystem>
#include <memory>

namespace tempest::assets
{
    class asset_loader
    {
      public:
        asset_loader() = default;
        asset_loader(const asset_loader&) = default;
        asset_loader(asset_loader&& other) noexcept = default;
        ~asset_loader() = default;

        asset_loader& operator=(const asset_loader&) = default;
        asset_loader& operator=(asset_loader&& rhs) noexcept = default;

        virtual bool load(const std::filesystem::path& path, void* dest) = 0;
        // std::unique_ptr<asset> load_async();

        virtual bool release(std::unique_ptr<asset>&& asset) = 0;
    };
} // namespace tempest::assets

#endif // tempest_assets_asset_loader_hpp