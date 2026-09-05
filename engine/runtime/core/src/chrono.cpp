#include <tempest/chrono.hpp>
#include <tempest/int.hpp>
#include <tempest/limits.hpp>
#include <tempest/ratio.hpp>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#ifdef small
#undef small
#endif
#else
#include <time.h>
#endif

namespace tempest::chrono
{
#ifdef _WIN32
    namespace
    {
        constexpr auto qpc_frequency_10mhz = 10000000LL;
        constexpr auto nanoseconds_per_second = 1000000000LL;
        constexpr auto nanoseconds_per_qpc_tick_10mhz = 100LL;
        constexpr auto nanoseconds_per_100ns_interval = 100LL;

        struct qpc_info
        {
            int64_t freq{0};
            int64_t mult{1};
            int64_t div{1};
            bool scale_100{false};
            bool scale_mult{false};
        };

        auto init_qpc() noexcept -> qpc_info
        {
            auto freq = LARGE_INTEGER{};
            QueryPerformanceFrequency(&freq);

            auto info = qpc_info{};
            info.freq = freq.QuadPart;

            // On modern Windows (Windows 10/11), QPC frequency is fixed at 10 MHz (1 tick = 100 ns).
            // Caching this precision scale avoids expensive 64-bit integer division instructions on every tick query.
            if (info.freq == qpc_frequency_10mhz)
            {
                info.scale_100 = true;
            }
            else if (info.freq > 0)
            {
                const auto gcd_val = ::tempest::detail::ratio_gcd(nanoseconds_per_second, info.freq);
                info.mult = nanoseconds_per_second / gcd_val;
                info.div = info.freq / gcd_val;
                if (info.div == 1)
                {
                    info.scale_mult = true;
                }
            }

            return info;
        }

        const auto s_qpc = init_qpc();
    } // namespace

    auto steady_clock::now() noexcept -> steady_clock::time_point
    {
        auto counter = LARGE_INTEGER{};
        QueryPerformanceCounter(&counter);
        const auto ticks = static_cast<int64_t>(counter.QuadPart);

        // Fast path 1: 10 MHz QPC frequency -> single 1-cycle multiply by 100
        if (s_qpc.scale_100)
        {
            return steady_clock::time_point(nanoseconds(ticks * nanoseconds_per_qpc_tick_10mhz));
        }

        // Fast path 2: frequency cleanly divides 1 GHz
        if (s_qpc.scale_mult)
        {
            return steady_clock::time_point(nanoseconds(ticks * s_qpc.mult));
        }

        // Fallback: 64-bit safe split conversion avoiding __udivti3 compiler-rt dependencies
        const auto sec = ticks / s_qpc.freq;
        const auto rem = ticks % s_qpc.freq;
        const auto nanosecs = (sec * nanoseconds_per_second) + ((rem * nanoseconds_per_second) / s_qpc.freq);
        return steady_clock::time_point(nanoseconds(nanosecs));
    }

    auto system_clock::now() noexcept -> system_clock::time_point
    {
        auto file_time = FILETIME{};
        GetSystemTimePreciseAsFileTime(&file_time);

        auto uli = ULARGE_INTEGER{};
        uli.LowPart = file_time.dwLowDateTime;
        uli.HighPart = file_time.dwHighDateTime;

        // Number of 100ns intervals between Jan 1, 1601 (Windows epoch) and Jan 1, 1970 (Unix epoch)
        constexpr auto epoch_offset_100ns = 116444736000000000ULL;
        const auto intervals_since_1970 = static_cast<int64_t>(uli.QuadPart - epoch_offset_100ns);
        const auto nanosecs = intervals_since_1970 * nanoseconds_per_100ns_interval;

        return system_clock::time_point(nanoseconds(nanosecs));
    }

#else

    auto steady_clock::now() noexcept -> steady_clock::time_point
    {
        auto ts = timespec{};
        clock_gettime(CLOCK_MONOTONIC, &ts);
        const auto nanosecs = static_cast<int64_t>(ts.tv_sec) * 1000000000LL + ts.tv_nsec;
        return steady_clock::time_point(nanoseconds(nanosecs));
    }

    auto system_clock::now() noexcept -> system_clock::time_point
    {
        auto ts = timespec{};
        clock_gettime(CLOCK_REALTIME, &ts);
        const auto nanosecs = static_cast<int64_t>(ts.tv_sec) * 1000000000LL + ts.tv_nsec;
        return system_clock::time_point(nanoseconds(nanosecs));
    }

#endif
} // namespace tempest::chrono
