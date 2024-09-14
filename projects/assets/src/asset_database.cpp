#include <tempest/asset_database.hpp>

#include <tempest/files.hpp>

namespace tempest::assets
{
    asset_database::asset_database(string root_path) : _root_path(std::move(root_path))
    {
    }

    void asset_database::register_importer(string extension, unique_ptr<asset_importer> importer)
    {
        _importers.insert({move(extension), move(importer)});
    }

    optional<reference_wrapper<prefab>> asset_database::load(string_view path, span<const byte> data)
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

        auto pre = make_unique<prefab>(tempest::move(context).get_prefab());
        auto prefab_path = string(path);
        auto& result = _prefabs[prefab_path] = tempest::move(pre);

        return tempest::ref(*result.get());
    }

    optional<reference_wrapper<prefab>> asset_database::load(string_view path)
    {
        auto full_path = _root_path;
        full_path.append(path);

        auto bytes = tempest::core::read_bytes(full_path);

        if (bytes.empty())
        {
            return none();
        }

        return load(path, bytes);
    }

    const flat_unordered_map<string, unique_ptr<prefab>>& asset_database::prefabs() const noexcept
    {
        return _prefabs;
    }
} // namespace tempest::assets