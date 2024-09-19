#ifndef tempest_guid_hpp
#define tempest_guid_hpp

#include <tempest/algorithm.hpp>
#include <tempest/array.hpp>
#include <tempest/hash.hpp>
#include <tempest/int.hpp>

namespace tempest
{
    struct guid
    {
        array<byte, 16> data;

        static guid generate_random_guid();
    };

    inline bool operator==(const guid& lhs, const guid& rhs) noexcept
    {
        return lhs.data == rhs.data;
    }

    inline bool operator!=(const guid& lhs, const guid& rhs) noexcept
    {
        return !(lhs == rhs);
    }

    template <>
    struct hash<guid>
    {
        [[nodiscard]] size_t operator()(const guid& g) const noexcept
        {
            uint64_t data_qwords[2];
            tempest::detail::copy_bytes(g.data.data(), data_qwords, sizeof(data_qwords));
            return detail::fnv1a64(data_qwords, 2);
        }
    };
};

#endif // tempest_guid_hpp