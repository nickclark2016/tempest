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

    auto to_local_time(system_clock::time_point timepoint) noexcept -> calendar_time
    {
        const auto ns = timepoint.time_since_epoch().count();
        if (ns <= 0)
        {
            return calendar_time{};
        }

        constexpr auto nanoseconds_per_100ns_interval = 100LL;
        constexpr auto epoch_offset_100ns = 116444736000000000ULL;

        const auto intervals_since_1970 = ns / nanoseconds_per_100ns_interval;
        const auto intervals_since_1601 = static_cast<uint64_t>(intervals_since_1970) + epoch_offset_100ns;

        auto uli = ULARGE_INTEGER{};
        uli.QuadPart = intervals_since_1601;

        auto file_time = FILETIME{};
        file_time.dwLowDateTime = uli.LowPart;
        file_time.dwHighDateTime = uli.HighPart;

        auto local_file_time = FILETIME{};
        if (FileTimeToLocalFileTime(&file_time, &local_file_time) == 0)
        {
            return calendar_time{};
        }

        auto st = SYSTEMTIME{};
        if (FileTimeToSystemTime(&local_file_time, &st) == 0)
        {
            return calendar_time{};
        }

        auto result = calendar_time{};
        result.year = static_cast<int32_t>(st.wYear);
        result.month = static_cast<int32_t>(st.wMonth);
        result.day = static_cast<int32_t>(st.wDay);
        result.hour = static_cast<int32_t>(st.wHour);
        result.minute = static_cast<int32_t>(st.wMinute);
        result.second = static_cast<int32_t>(st.wSecond);
        result.millisecond = static_cast<int32_t>(st.wMilliseconds);
        return result;
    }

    auto to_utc_time(system_clock::time_point timepoint) noexcept -> calendar_time
    {
        const auto ns = timepoint.time_since_epoch().count();
        if (ns <= 0)
        {
            return calendar_time{};
        }

        constexpr auto nanoseconds_per_100ns_interval = 100LL;
        constexpr auto epoch_offset_100ns = 116444736000000000ULL;

        const auto intervals_since_1970 = ns / nanoseconds_per_100ns_interval;
        const auto intervals_since_1601 = static_cast<uint64_t>(intervals_since_1970) + epoch_offset_100ns;

        auto uli = ULARGE_INTEGER{};
        uli.QuadPart = intervals_since_1601;

        auto file_time = FILETIME{};
        file_time.dwLowDateTime = uli.LowPart;
        file_time.dwHighDateTime = uli.HighPart;

        auto st = SYSTEMTIME{};
        if (FileTimeToSystemTime(&file_time, &st) == 0)
        {
            return calendar_time{};
        }

        auto result = calendar_time{};
        result.year = static_cast<int32_t>(st.wYear);
        result.month = static_cast<int32_t>(st.wMonth);
        result.day = static_cast<int32_t>(st.wDay);
        result.hour = static_cast<int32_t>(st.wHour);
        result.minute = static_cast<int32_t>(st.wMinute);
        result.second = static_cast<int32_t>(st.wSecond);
        result.millisecond = static_cast<int32_t>(st.wMilliseconds);
        return result;
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

    auto to_local_time(system_clock::time_point timepoint) noexcept -> calendar_time
    {
        const auto ns = timepoint.time_since_epoch().count();
        if (ns <= 0)
        {
            return calendar_time{};
        }

        const auto sec = static_cast<time_t>(ns / 1000000000LL);
        const auto ms = static_cast<int32_t>((ns % 1000000000LL) / 1000000LL);

        auto tm_val = tm{};
        if (localtime_r(&sec, &tm_val) == nullptr)
        {
            return calendar_time{};
        }

        auto result = calendar_time{};
        result.year = tm_val.tm_year + 1900;
        result.month = tm_val.tm_mon + 1;
        result.day = tm_val.tm_mday;
        result.hour = tm_val.tm_hour;
        result.minute = tm_val.tm_min;
        result.second = tm_val.tm_sec;
        result.millisecond = ms;
        return result;
    }

    auto to_utc_time(system_clock::time_point timepoint) noexcept -> calendar_time
    {
        const auto ns = timepoint.time_since_epoch().count();
        if (ns <= 0)
        {
            return calendar_time{};
        }

        const auto sec = static_cast<time_t>(ns / 1000000000LL);
        const auto ms = static_cast<int32_t>((ns % 1000000000LL) / 1000000LL);

        auto tm_val = tm{};
        if (gmtime_r(&sec, &tm_val) == nullptr)
        {
            return calendar_time{};
        }

        auto result = calendar_time{};
        result.year = tm_val.tm_year + 1900;
        result.month = tm_val.tm_mon + 1;
        result.day = tm_val.tm_mday;
        result.hour = tm_val.tm_hour;
        result.minute = tm_val.tm_min;
        result.second = tm_val.tm_sec;
        result.millisecond = ms;
        return result;
    }

#endif
} // namespace tempest::chrono
