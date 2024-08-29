#ifndef tempest_assets_asset_importer_hpp
#define tempest_assets_asset_importer_hpp

#include <tempest/asset.hpp>
#include <tempest/prefab.hpp>

#include <tempest/int.hpp>
#include <tempest/memory.hpp>
#include <tempest/span.hpp>
#include <tempest/string.hpp>
#include <tempest/string_view.hpp>

namespace tempest::assets
{
    class asset_import_context
    {
      public:
        explicit asset_import_context(span<const byte> data);
        asset_import_context(string path, span<const byte> data);

        string_view path() const noexcept;
        span<const byte> data() const noexcept;

        const prefab& get_prefab() const& noexcept;
        prefab get_prefab() && noexcept;

        void add_asset_as_primary(unique_ptr<asset> asset);
        void add_asset(unique_ptr<asset> asset);

      private:
        string _path;
        span<const byte> _data;
        prefab _prefab;
        asset* _primary_asset{nullptr};
    };

    class asset_importer
    {
      public:
        asset_importer() = default;
        asset_importer(const asset_importer&) = delete;
        asset_importer(asset_importer&&) noexcept = delete;
        virtual ~asset_importer() = default;

        asset_importer& operator=(const asset_importer&) = delete;
        asset_importer& operator=(asset_importer&&) noexcept = delete;

        virtual void on_asset_load(asset_import_context& context) = 0;
    };
} // namespace tempest::assets

#endif // tempest_assets_asset_importer_hpp