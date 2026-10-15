#ifndef tempest_assets_importers_render_target_importer_hpp
#define tempest_assets_importers_render_target_importer_hpp

#include <tempest/asset_importer.hpp>
#include <tempest/texture.hpp>

namespace tempest::assets
{
    class render_target_importer : public asset_importer
    {
      public:
        explicit render_target_importer(core::texture_registry* tex_reg);
        auto import(asset_database& db, span<const byte> data, ecs::archetype_registry& registry,
                    optional<string_view> path) -> ecs::entity override;

      private:
        core::texture_registry* _texture_reg;
    };
} // namespace tempest::assets

#endif // tempest_assets_importers_render_target_importer_hpp
