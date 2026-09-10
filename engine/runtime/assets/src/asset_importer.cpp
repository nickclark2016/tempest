#include <tempest/asset_importer.hpp>

#include <tempest/files.hpp>

namespace tempest::assets
{
    auto asset_importer::import(asset_database& asset_db, string_view path, ecs::archetype_registry& registry)
        -> ecs::entity
    {
        auto bytes = read_file_to_vector(path).value_or(vector<byte>{});
        return import(asset_db, bytes, registry, some(path));
    }
} // namespace tempest::assets