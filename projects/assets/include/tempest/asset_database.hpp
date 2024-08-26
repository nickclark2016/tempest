#ifndef tempest_assets_asset_database_hpp
#define tempest_assets_asset_database_hpp

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

      private:
        string _root_path;
    };
} // namespace tempest::assets

#endif