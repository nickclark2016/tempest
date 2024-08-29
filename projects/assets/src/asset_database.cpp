#include <tempest/asset_database.hpp>

namespace tempest::assets
{
    asset_database::asset_database(string root_path) : _root_path(std::move(root_path))
    {
    }

    void asset_database::register_importer(string extension, unique_ptr<asset_importer> importer)
    {
        _importers.insert({move(extension), move(importer)});
    }

    optional<prefab> asset_database::load(string_view path, span<const byte> data) const
    {
        auto period_location = tempest::search_last_of(path, '.');
        auto extension = span(period_location + 1, path.end());

        auto it = _importers.find(string(extension.begin(), extension.end()));
        if (it == _importers.end())
        {
            return none();
        }

        asset_import_context context(path, data);
        it->second->on_asset_load(context);

    return some(move(context).get_prefab());
    }
}