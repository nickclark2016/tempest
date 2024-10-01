#include <tempest/asset_importer.hpp>

#include <tempest/files.hpp>

namespace tempest::assets
{
    ecs::entity asset_importer::import(asset_database& db, string_view path, ecs::registry& registry)
    {
        auto bytes = core::read_bytes(path);
        return import(db, bytes, registry, some(path));
    }
}