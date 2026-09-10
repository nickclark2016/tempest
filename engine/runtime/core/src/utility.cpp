#include <tempest/utility.hpp>

#include <cstdlib>
#include <cstring>

namespace tempest
{
    void abort() noexcept
    {
        ::abort();
    }

    auto memcpy(void* dst, const void* src, size_t count) noexcept -> void*
    {
        return ::memcpy(dst, src, count);
    }

    auto memset(void* dst, int ch, size_t count) noexcept -> void*
    {
        return ::memset(dst, ch, count);
    }
} // namespace tempest