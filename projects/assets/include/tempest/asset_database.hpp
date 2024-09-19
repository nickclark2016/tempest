#ifndef tempest_assets_asset_database_hpp
#define tempest_assets_asset_database_hpp

#include <tempest/asset_importer.hpp>
#include <tempest/flat_unordered_map.hpp>
#include <tempest/memory.hpp>
#include <tempest/string.hpp>

namespace tempest::assets
{
    class asset_database
    {
      public:
        void register_importer(unique_ptr<asset_importer> importer, string_view extension);
        [[nodiscard]] ecs::entity import(string_view path, ecs::registry& registry);

      private:
        flat_unordered_map<string, unique_ptr<asset_importer>> _importers;
    };
} // namespace tempest::assets

#endif // tempest_assets_asset_database_hpp