#ifndef tempest_assets_importers_glb_importer_hpp
#define tempest_assets_importers_glb_importer_hpp

#include <tempest/asset_importer.hpp>
#include <tempest/material.hpp>
#include <tempest/optional.hpp>
#include <tempest/span.hpp>
#include <tempest/texture.hpp>
#include <tempest/vertex.hpp>

namespace tempest::assets
{
    class glb_importer : public asset_importer
    {
      public:
        glb_importer(core::mesh_registry* mesh_reg, core::texture_registry* texture_reg,
                     core::material_registry* material_reg) noexcept;

        auto import(asset_database& asset_db, span<const byte> bytes, ecs::archetype_registry& registry,
                    optional<string_view> path) -> ecs::archetype_entity override;

      private:
        core::mesh_registry* _mesh_reg;
        core::texture_registry* _texture_reg;
        core::material_registry* _material_reg;
    };
} // namespace tempest::assets

#endif // tempest_assets_importers_glb_importer_hpp
