#ifndef tempest_assets_asset_importer_hpp
#define tempest_assets_asset_importer_hpp

#include <tempest/api.hpp>
#include <tempest/archetype.hpp>
#include <tempest/int.hpp>
#include <tempest/optional.hpp>
#include <tempest/span.hpp>
#include <tempest/string_view.hpp>

namespace tempest::assets
{
    class asset_database;

    class TEMPEST_API asset_importer
    {
      public:
        asset_importer() noexcept = default;
        asset_importer(const asset_importer&) = delete;
        asset_importer(asset_importer&&) noexcept = delete;
        virtual ~asset_importer() = default;

        asset_importer& operator=(const asset_importer&) = delete;     // NOLINT
        asset_importer& operator=(asset_importer&&) noexcept = delete; // NOLINT

        [[nodiscard]] virtual auto import(asset_database& asset_db, string_view path, ecs::archetype_registry& registry)
            -> ecs::archetype_entity;
        [[nodiscard]] virtual auto import(asset_database& asset_db, span<const byte> data,
                                          ecs::archetype_registry& registry, optional<string_view> asset_path)
            -> ecs::archetype_entity = 0;
    };
} // namespace tempest::assets

#endif // tempest_assets_asset_importer_hpp