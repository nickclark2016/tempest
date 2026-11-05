#include <tempest/algorithm.hpp>
#include <tempest/utility.hpp>

#if defined(__clang__) || defined(__GNUC__)

namespace tempest::detail
{
    void copy_bytes(const void* src, void* dest, size_t count)
    {
        tempest::memcpy(dest, src, count);
    }

    auto compare_bytes(const void* lhs, const void* rhs, size_t count) -> int
    {
        return __builtin_memcmp(lhs, rhs, count);
    }
} // namespace tempest::detail

#else

extern "C" int memcmp(const void* lhs, const void* rhs, tempest::size_t count);

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

#endif
