#ifndef tempest_assets_asset_hpp
#define tempest_assets_asset_hpp

#include <tempest/guid.hpp>
#include <tempest/string_view.hpp>

namespace tempest::assets
{
    class asset
    {
      public:
        asset() = default;
        asset(const asset&) = delete;
        asset(asset&&) noexcept = delete;
        virtual ~asset() = default;

        asset& operator=(const asset&) = delete;
        asset& operator=(asset&&) noexcept = delete;

        virtual string_view name() const noexcept = 0;
        virtual guid id() const noexcept = 0;
    };
} // namespace tempest::assets

#endif // tempest_assets_asset_hpp