#ifndef tempest_assets_asset_database_hpp
#define tempest_assets_asset_database_hpp

#include <tempest/asset_importer.hpp>

#include <tempest/flat_unordered_map.hpp>
#include <tempest/memory.hpp>
#include <tempest/optional.hpp>
#include <tempest/prefab.hpp>
#include <tempest/string.hpp>

namespace tempest::assets
{
    class asset_database
    {
      public:
        explicit asset_database(string root_path);

        asset_database(const asset_database&) = delete;
        asset_database(asset_database&&) noexcept = delete;
        asset_database& operator=(const asset_database&) = delete;
        asset_database& operator=(asset_database&&) noexcept = delete;

        ~asset_database() = default;

        void register_importer(string extension, unique_ptr<asset_importer> importer);

        [[nodiscard]] optional<reference_wrapper<prefab>> load(string_view path, span<const byte> data);
        [[nodiscard]] optional<reference_wrapper<prefab>> load(string_view path);

        [[nodiscard]] const flat_unordered_map<string, unique_ptr<prefab>>& prefabs() const noexcept;

      private:
        string _root_path;

        flat_unordered_map<string, unique_ptr<asset_importer>> _importers;
        flat_unordered_map<string, unique_ptr<prefab>> _prefabs;
    };
} // namespace tempest::assets

#endif