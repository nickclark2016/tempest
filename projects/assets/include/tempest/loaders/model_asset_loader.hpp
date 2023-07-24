#ifndef tempest_assets_model_asset_loader_hpp
#define tempest_assets_model_asset_loader_hpp

#include <tempest/assets.hpp>
#include <tempest/assets/model_asset.hpp>

#include <filesystem>
#include <memory>

namespace tempest::assets
{
    class model_asset_loader : public asset_loader
    {
      public:
        model_asset_loader(asset_pool* mesh_pool, asset_pool* material_pool);

        bool load(const std::filesystem::path& path, void* dest) override;
        bool release(std::unique_ptr<asset>&& asset) override;

        using type = model_asset;

      private:
        asset_pool* _mesh_asset_pool;
        asset_pool* _material_asset_pool;
    };
} // namespace tempest::assets

#endif // tempest_assets_model_asset_loader_hpp