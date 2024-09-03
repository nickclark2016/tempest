#ifndef tempest_assets_gltf_importer_hpp
#define tempest_assets_gltf_importer_hpp

#include <tempest/asset_importer.hpp>

namespace tempest::assets
{
    class gltf_importer : public asset_importer
    {
      public:
        void on_asset_load(asset_import_context& context) override;
    };
} // namespace tempest::assets

#endif // tempest_assets_gltf_importer_hpp