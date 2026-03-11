#include <tempest/asset_importer.hpp>

#include <tempest/files.hpp>

namespace tempest::assets
{
    auto asset_importer::import(asset_database& asset_db, string_view path, ecs::archetype_registry& registry) -> ecs::archetype_entity
    {
        auto bytes = core::read_bytes(path);
        return import(asset_db, bytes, registry, some(path));
    }
}