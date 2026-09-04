#include <tempest/array.hpp>
#include <tempest/vk/calibration.hpp>

#if defined(TEMPEST_PLATFORM_WINDOWS) || defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#elif defined(TEMPEST_PLATFORM_LINUX) || defined(__linux__)
#include <time.h>
#else
#include <chrono>
#endif

namespace tempest::rhi::vk
{
    timeline_calibrator::timeline_calibrator(float timestamp_period_ns) noexcept
        : _timestamp_period_ns{timestamp_period_ns > 0.0F ? timestamp_period_ns : 1.0F}, _gpu_to_cpu_offset_ns{0},
          _is_calibrated{false}, _is_hardware_calibrated{false},
#if defined(TEMPEST_PLATFORM_WINDOWS) || defined(_WIN32)
          _host_time_domain{VK_TIME_DOMAIN_QUERY_PERFORMANCE_COUNTER_EXT}
#elif defined(TEMPEST_PLATFORM_LINUX) || defined(__linux__)
          _host_time_domain{VK_TIME_DOMAIN_CLOCK_MONOTONIC_RAW_EXT}
#else
          _host_time_domain{VK_TIME_DOMAIN_DEVICE_EXT}
#endif
    {
    }

    timeline_calibrator::timeline_calibrator(float timestamp_period_ns, const vkb::DispatchTable& dispatch,
                                             VkDevice device) noexcept
        : timeline_calibrator{timestamp_period_ns}
    {
        calibrate(dispatch, device);
    }

    auto timeline_calibrator::query_cpu_timestamp_ns() noexcept -> uint64_t
    {
#if defined(TEMPEST_PLATFORM_WINDOWS) || defined(_WIN32)
        static const auto frequency = []() {
            LARGE_INTEGER freq;
            QueryPerformanceFrequency(&freq);
            return static_cast<uint64_t>(freq.QuadPart);
        }();
        LARGE_INTEGER counter;
        QueryPerformanceCounter(&counter);
        const auto count = static_cast<uint64_t>(counter.QuadPart);
        return (count / frequency) * 1'000'000'000ULL + ((count % frequency) * 1'000'000'000ULL) / frequency;
#elif defined(TEMPEST_PLATFORM_LINUX) || defined(__linux__)
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
        return static_cast<uint64_t>(ts.tv_sec) * 1'000'000'000ULL + static_cast<uint64_t>(ts.tv_nsec);
#else
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch())
                .count());
#endif
    }

    auto timeline_calibrator::calibrate(const vkb::DispatchTable& dispatch, VkDevice device) noexcept -> bool
    {
        if (dispatch.fp_vkGetCalibratedTimestampsEXT == nullptr && dispatch.fp_vkGetCalibratedTimestampsKHR == nullptr)
        {
            return false;
        }

        const auto timestamp_infos = array<VkCalibratedTimestampInfoEXT, 2>{
            VkCalibratedTimestampInfoEXT{
                .sType = VK_STRUCTURE_TYPE_CALIBRATED_TIMESTAMP_INFO_EXT,
                .pNext = nullptr,
                .timeDomain = _host_time_domain,
            },
            VkCalibratedTimestampInfoEXT{
                .sType = VK_STRUCTURE_TYPE_CALIBRATED_TIMESTAMP_INFO_EXT,
                .pNext = nullptr,
                .timeDomain = VK_TIME_DOMAIN_DEVICE_EXT,
            },
        };

        auto timestamps = array<uint64_t, 2>{0, 0};
        auto max_deviation = uint64_t{0};
        auto res = VK_ERROR_INITIALIZATION_FAILED;

        if (dispatch.fp_vkGetCalibratedTimestampsEXT != nullptr)
        {
            res = dispatch.getCalibratedTimestampsEXT(2, timestamp_infos.data(), timestamps.data(), &max_deviation);
        }
        else if (dispatch.fp_vkGetCalibratedTimestampsKHR != nullptr)
        {
            res = dispatch.getCalibratedTimestampsKHR(
                2, reinterpret_cast<const VkCalibratedTimestampInfoKHR*>(timestamp_infos.data()), timestamps.data(),
                &max_deviation);
        }

        if (res == VK_SUCCESS)
        {
            auto host_ns = uint64_t{0};
#if defined(TEMPEST_PLATFORM_WINDOWS) || defined(_WIN32)
            static const auto frequency = []() {
                LARGE_INTEGER freq;
                QueryPerformanceFrequency(&freq);
                return static_cast<uint64_t>(freq.QuadPart);
            }();
            const auto count = timestamps[0];
            host_ns = (count / frequency) * 1'000'000'000ULL + ((count % frequency) * 1'000'000'000ULL) / frequency;
#else
            host_ns = timestamps[0];
#endif
            const auto gpu_ns = gpu_ticks_to_ns(timestamps[1]);
            _gpu_to_cpu_offset_ns = static_cast<int64_t>(host_ns) - static_cast<int64_t>(gpu_ns);
            _is_calibrated = true;
            _is_hardware_calibrated = true;
            return true;
        }

        return false;
    }

    auto timeline_calibrator::calibrate_fallback(uint64_t cpu_bracket_start_ns, uint64_t cpu_bracket_end_ns,
                                                 uint64_t gpu_ticks) noexcept -> void
    {
        const auto avg_cpu_ns = cpu_bracket_end_ns >= cpu_bracket_start_ns
                                    ? cpu_bracket_start_ns + (cpu_bracket_end_ns - cpu_bracket_start_ns) / 2
                                    : cpu_bracket_end_ns + (cpu_bracket_start_ns - cpu_bracket_end_ns) / 2;
        const auto gpu_ns = gpu_ticks_to_ns(gpu_ticks);
        _gpu_to_cpu_offset_ns = static_cast<int64_t>(avg_cpu_ns) - static_cast<int64_t>(gpu_ns);
        _is_calibrated = true;
        _is_hardware_calibrated = false;
    }

    auto timeline_calibrator::gpu_ticks_to_ns(uint64_t gpu_ticks) const noexcept -> uint64_t
    {
        return static_cast<uint64_t>(static_cast<double>(gpu_ticks) * static_cast<double>(_timestamp_period_ns));
    }

    auto timeline_calibrator::convert_gpu_timestamp_to_cpu_ns(uint64_t gpu_ticks) const noexcept -> uint64_t
    {
        const auto gpu_ns = gpu_ticks_to_ns(gpu_ticks);
        const auto adjusted_ns = static_cast<int64_t>(gpu_ns) + _gpu_to_cpu_offset_ns;
        return adjusted_ns >= 0 ? static_cast<uint64_t>(adjusted_ns) : 0ULL;
    }
} // namespace tempest::rhi::vk
