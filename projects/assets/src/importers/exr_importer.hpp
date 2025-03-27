#ifndef tempest_assets_importers_exr_importer_hpp
#define tempest_assets_importers_exr_importer_hpp

#include <tempest/asset_importer.hpp>

#include <tempest/texture.hpp>

namespace tempest::assets
{
    class exr_importer : public asset_importer
    {
      public:
        explicit exr_importer(core::texture_registry* tex_reg);
        ecs::archetype_entity import(asset_database& db, span<const byte> data, ecs::archetype_registry& registry,
                                     optional<string_view> path) override;

      private:
        core::texture_registry* _texture_reg;
    };
} // namespace tempest::assets

#endif // tempest_assets_importers_exr_importer_hpp
