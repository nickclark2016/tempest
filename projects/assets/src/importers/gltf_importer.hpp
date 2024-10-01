#ifndef tempest_assets_importers_gltf_importer_hpp
#define tempest_assets_importers_gltf_importer_hpp

#include <tempest/asset_importer.hpp>
#include <tempest/material.hpp>
#include <tempest/texture.hpp>
#include <tempest/vertex.hpp>

namespace tempest::assets
{
    class gltf_importer : public asset_importer
    {
      public:
        gltf_importer(core::mesh_registry* mesh_reg, core::texture_registry* texture_reg, core::material_registry* material_reg) noexcept;
        ecs::entity import(asset_database& db, span<const byte> bytes, ecs::registry& registry, optional<string_view> path) override;

      private:
        core::mesh_registry* _mesh_reg;
        core::texture_registry* _texture_reg;
        core::material_registry* _material_reg;
    };
} // namespace tempest::assets

#endif // tempest_assets_importers_gltf_importer_hpp