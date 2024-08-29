#include <tempest/asset_importer.hpp>

namespace tempest::assets
{
    asset_import_context::asset_import_context(span<const byte> data) : _data{data}
    {
    }

    asset_import_context::asset_import_context(string path, span<const byte> data)
        : _path{tempest::move(path)}, _data{data}
    {
    }

    string_view asset_import_context::path() const noexcept
    {
        return _path;
    }

    span<const byte> asset_import_context::data() const noexcept
    {
        return _data;
    }

    const prefab& asset_import_context::get_prefab() const& noexcept
    {
        return _prefab;
    }

    prefab asset_import_context::get_prefab() && noexcept
    {
        return tempest::move(_prefab);
    }

    void asset_import_context::add_asset_as_primary(unique_ptr<asset> asset)
    {
        _primary_asset = asset.get();
        _prefab.assets.push_back(tempest::move(asset));
    }

    void asset_import_context::add_asset(unique_ptr<asset> asset)
    {
        _prefab.assets.push_back(tempest::move(asset));
    }
} // namespace tempest::assets