#ifndef tempest_guid_hpp
#define tempest_guid_hpp

#include <tempest/api.hpp>
#include <tempest/algorithm.hpp>
#include <tempest/array.hpp>
#include <tempest/hash.hpp>
#include <tempest/int.hpp>
#include <tempest/string.hpp>

namespace tempest
{
    struct TEMPEST_API guid
    {
        static constexpr size_t size = 16;

        array<byte, size> data;

        static auto generate_random_guid() -> guid;
    };

    TEMPEST_API
    inline bool operator==(const guid& lhs, const guid& rhs) noexcept // NOLINT(modernize-use-trailing-return-type)
    {
        return lhs.data == rhs.data;
    }

    TEMPEST_API
    inline bool operator!=(const guid& lhs, const guid& rhs) noexcept // NOLINT(modernize-use-trailing-return-type)
    {
        return !(lhs == rhs);
    }

    template <>
    struct hash<guid>
    {
        [[nodiscard]] size_t operator()(const guid& g) const noexcept // NOLINT(modernize-use-trailing-return-type)
        {
            uint64_t data_qwords[2]; // NOLINT(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
            tempest::detail::copy_bytes(g.data.data(), data_qwords, sizeof(data_qwords));
            return detail::fnv1a64(data_qwords, 2);
        }
    };

    [[nodiscard]] TEMPEST_API auto to_string(const guid& uid) -> string;
};

#endif // tempest_guid_hpp