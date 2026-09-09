#include <tempest/algorithm.hpp>

#include <tempest/utility.hpp>

#include <cstring>

namespace tempest::detail
{
    void copy_bytes(const void* src, void* dest, size_t count)
    {
        tempest::memcpy(dest, src, count);
    }

    auto compare_bytes(const void* lhs, const void* rhs, size_t count) -> int
    {
        return ::memcmp(lhs, rhs, count);
    }
} // namespace tempest::detail
