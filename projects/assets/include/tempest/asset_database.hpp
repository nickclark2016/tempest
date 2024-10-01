#ifndef tempest_assets_asset_database_hpp
#define tempest_assets_asset_database_hpp

#include <tempest/asset_importer.hpp>
#include <tempest/flat_unordered_map.hpp>
#include <tempest/guid.hpp>
#include <tempest/material.hpp>
#include <tempest/memory.hpp>
#include <tempest/optional.hpp>
#include <tempest/string.hpp>
#include <tempest/texture.hpp>
#include <tempest/vertex.hpp>

namespace tempest::assets
{
    struct prefab_tag_t{};

    inline constexpr prefab_tag_t prefab_tag{};

    class asset_database
    {
      public:
        struct asset_metadata
        {
            string path;
            flat_unordered_map<string, string> metadata;
        };

        asset_database(core::mesh_registry* mesh_reg, core::texture_registry* texture_reg,
                       core::material_registry* material_reg) noexcept;

        void register_importer(unique_ptr<asset_importer> importer, string_view extension);
        [[nodiscard]] ecs::entity import(string_view path, ecs::registry& registry);
        [[nodiscard]] guid register_asset_metadata(asset_metadata meta);
        [[nodiscard]] optional<const asset_metadata&> get_asset_metadata(guid id) const;

      private:
        flat_unordered_map<string, unique_ptr<asset_importer>> _importers;
        flat_unordered_map<guid, asset_metadata> _metadata;

        core::mesh_registry* _mesh_reg;
        core::texture_registry* _texture_reg;
        core::material_registry* _material_reg;
    };

    struct asset_metadata_component
    {
        guid metadata_id;
    };
} // namespace tempest::assets

namespace tempest::ecs
{
    template <>
    struct is_duplicatable<assets::prefab_tag_t> : false_type
    {
    };
}

#endif // tempest_assets_asset_database_hpp