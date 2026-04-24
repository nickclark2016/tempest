#ifndef tempest_assets_asset_type_id_hpp
#define tempest_assets_asset_type_id_hpp

#include <tempest/hash.hpp>
#include <tempest/int.hpp>
#include <tempest/meta.hpp>

namespace tempest::assets
{
    class asset_type_id
    {
      public:
        constexpr asset_type_id() noexcept = default;

        template <typename T>
        static constexpr asset_type_id of() noexcept
        {
            return asset_type_id{core::type_hash<T>::value()};
        }

        static constexpr asset_type_id from_hash(size_t hash_value) noexcept
        {
            return asset_type_id{hash_value};
        }

        constexpr size_t hash() const noexcept
        {
            return _hash;
        }

        constexpr bool operator==(const asset_type_id& other) const noexcept
        {
            return _hash == other._hash;
        }

        constexpr bool operator!=(const asset_type_id& other) const noexcept
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
        [[nodiscard]] size_t operator()(const assets::asset_type_id& type_id) const noexcept
        {
            return hash<size_t>{}(type_id.hash());
        }
    };
} // namespace tempest

#endif // tempest_assets_asset_type_id_hpp
