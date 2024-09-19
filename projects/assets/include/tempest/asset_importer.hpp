#ifndef tempest_assets_asset_importer_hpp
#define tempest_assets_asset_importer_hpp

#include <tempest/int.hpp>
#include <tempest/registry.hpp>
#include <tempest/span.hpp>
#include <tempest/string_view.hpp>

namespace tempest::assets
{
    class asset_importer
    {
      public:
        asset_importer() noexcept = default;
        asset_importer(const asset_importer&) = delete;
        asset_importer(asset_importer&&) noexcept = delete;
        virtual ~asset_importer() = default;

        asset_importer& operator=(const asset_importer&) = delete;
        asset_importer& operator=(asset_importer&&) noexcept = delete;

        [[nodiscard]] virtual ecs::entity import(string_view path, ecs::registry& registry);
        [[nodiscard]] virtual ecs::entity import(span<const byte> data, ecs::registry& registry) = 0;
    };
} // namespace tempest::assets

#endif // tempest_assets_asset_importer_hpp