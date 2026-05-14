#include "tempest/int.hpp"
#include <tempest/atomic.hpp>

namespace tempest
{
    namespace detail
    {
#if defined(TEMPEST_PLATFORM_LINUX)
        constexpr auto proxy_futex_table_size = 256;
        uint32_t g_futex_table[proxy_futex_table_size] = {};

        auto get_proxy_futex(const void* addr) -> int32_t*
        {
            const auto val = reinterpret_cast<uintptr_t>(addr);
            const auto idx = ((val >> 3) * 2654435761u) & (proxy_futex_table_size - 1);
            return reinterpret_cast<int32_t*>(&g_futex_table[idx]);
        }
#endif
    } // namespace detail
} // namespace tempest