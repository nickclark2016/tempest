#ifndef tempest_assets_asset_hpp
#define tempest_assets_asset_hpp

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
    };
}

#endif // tempest_assets_asset_hpp