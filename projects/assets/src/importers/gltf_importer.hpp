#ifndef tempest_assets_importers_gltf_importer_hpp
#define tempest_assets_importers_gltf_importer_hpp

#include <tempest/asset_importer.hpp>

namespace tempest::assets
{
    class gltf_importer : public asset_importer
    {
      public:
        ecs::entity import(string_view path, ecs::registry& registry) override;
    };
} // namespace tempest::assets

#endif // tempest_assets_importers_gltf_importer_hpp