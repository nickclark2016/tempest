#include <tempest/utility.hpp>

#if defined(__clang__) || defined(__GNUC__)

namespace tempest
{
    void abort() noexcept
    {
        __builtin_trap();
    }

    auto memcpy(void* dst, const void* src, size_t count) noexcept -> void*
    {
        return __builtin_memcpy(dst, src, count);
    }

    auto memset(void* dst, int ch, size_t count) noexcept -> void*
    {
        return __builtin_memset(dst, ch, count);
    }
} // namespace tempest

#else

extern "C"
{
    [[noreturn]] void abort();
    void* memcpy(void* dst, const void* src, tempest::size_t count);
    void* memset(void* dst, int ch, tempest::size_t count);
}

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

#endif