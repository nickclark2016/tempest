#include <tempest/asset_database.hpp>

namespace tempest::assets
{
    void asset_database::register_importer(unique_ptr<asset_importer> importer, string_view extension)
    {
        _importers[string(extension)] = move(importer);
    }

    ecs::entity asset_database::import(string_view path, ecs::registry& registry)
    {
        auto extension_it = search_last_of(path, '.');
        if (extension_it == path.end())
        {
            return ecs::null;
        }

        auto importer_it = _importers.find(string(extension_it, path.end()));
        if (importer_it != _importers.end())
        {
            return importer_it->second->import(path, registry);
        }

        return ecs::null;
    }
}