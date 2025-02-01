#ifndef tempest_assets_asset_importer_hpp
#define tempest_assets_asset_importer_hpp

#include <tempest/archetype.hpp>
#include <tempest/int.hpp>
#include <tempest/optional.hpp>
#include <tempest/span.hpp>
#include <tempest/string_view.hpp>

namespace tempest::assets
{
    class asset_database;

    class asset_importer
    {
      public:
        asset_importer() noexcept = default;
        asset_importer(const asset_importer&) = delete;
        asset_importer(asset_importer&&) noexcept = delete;
        virtual ~asset_importer() = default;

        asset_importer& operator=(const asset_importer&) = delete;
        asset_importer& operator=(asset_importer&&) noexcept = delete;

        [[nodiscard]] virtual ecs::archetype_entity import(asset_database& db, string_view path,
                                                           ecs::archetype_registry& registry);
        [[nodiscard]] virtual ecs::archetype_entity import(asset_database& db, span<const byte> data,
                                                           ecs::archetype_registry& registry,
                                                           optional<string_view> asset_path) = 0;
    };
} // namespace tempest::assets

#endif // tempest_assets_asset_importer_hpp