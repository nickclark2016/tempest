#ifndef tempest_assets_asset_hpp
#define tempest_assets_asset_hpp

#include <string>
#include <string_view>
#include <optional>
#include <filesystem>
#include <cstdint>

namespace tempest::assets
{
    struct asset_metadata
    {
        std::string name;
        std::uint64_t uuid;
        std::optional<std::filesystem::path> path;
    };

    class asset
    {
      public:
        asset(std::string_view name)
        {
            _metadata.name = name;
        }
        asset(const asset&) = delete;
        asset(asset&& other) noexcept = default;
        ~asset() = default;

        asset& operator=(const asset&) = delete;
        asset& operator=(asset&& rhs) noexcept = default;

      private:
        friend class asset_manager;

        asset_metadata _metadata;
    };
} // namespace tempest::assets
#endif // tempest_assets_asset_hpp