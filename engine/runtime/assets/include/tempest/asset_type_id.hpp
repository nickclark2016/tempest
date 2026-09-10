#ifndef tempest_assets_asset_type_id_hpp
#define tempest_assets_asset_type_id_hpp

#include <tempest/api.hpp>
#include <tempest/hash.hpp>
#include <tempest/int.hpp>
#include <tempest/meta.hpp>

namespace tempest::assets
{
    class TEMPEST_API asset_type_id
    {
      public:
        constexpr asset_type_id() noexcept = default;

        template <typename T>
        static constexpr auto of() noexcept -> asset_type_id
        {
            return asset_type_id{core::type_hash<T>::value()};
        }

        static constexpr auto from_hash(size_t hash_value) noexcept -> asset_type_id
        {
            return asset_type_id{hash_value};
        }

        [[nodiscard]] constexpr auto hash() const noexcept -> size_t
        {
            return _hash;
        }

        constexpr auto operator==(const asset_type_id& other) const noexcept -> bool
        {
            return _hash == other._hash;
        }

        constexpr auto operator!=(const asset_type_id& other) const noexcept -> bool
        {
            return _hash != other._hash;
        }

      private:
        size_t _hash{0};

        explicit constexpr asset_type_id(size_t hash_value) noexcept : _hash{hash_value}
        {
        }
    };
} // namespace tempest::assets

namespace tempest
{
    template <>
    struct hash<assets::asset_type_id>
    {
        [[nodiscard]] auto operator()(const assets::asset_type_id& type_id) const noexcept -> size_t
        {
            return hash<size_t>{}(type_id.hash());
        }
    };
} // namespace tempest

#endif // tempest_assets_asset_type_id_hpp
