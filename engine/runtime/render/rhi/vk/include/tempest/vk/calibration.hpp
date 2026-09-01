#ifndef tempest_rhi_vk_calibration_hpp
#define tempest_rhi_vk_calibration_hpp

#include <tempest/api.hpp>
#include <tempest/int.hpp>

#include <VkBootstrapDispatch.h>
#include <vulkan/vulkan_core.h>

namespace tempest::rhi::vk
{
    class TEMPEST_API timeline_calibrator
    {
      public:
        timeline_calibrator() = delete;
        explicit timeline_calibrator(float timestamp_period_ns) noexcept;
        timeline_calibrator(float timestamp_period_ns, const vkb::DispatchTable& dispatch, VkDevice device) noexcept;

        auto calibrate(const vkb::DispatchTable& dispatch, VkDevice device) noexcept -> bool;

        auto calibrate_fallback(uint64_t cpu_bracket_start_ns, uint64_t cpu_bracket_end_ns, uint64_t gpu_ticks) noexcept
            -> void;

        [[nodiscard]] auto get_timestamp_period_ns() const noexcept -> float
        {
            return _timestamp_period_ns;
        }

        [[nodiscard]] auto get_calibrated_offset_ns() const noexcept -> uint64_t
        {
            return _gpu_to_cpu_offset_ns >= 0 ? static_cast<uint64_t>(_gpu_to_cpu_offset_ns) : 0;
        }

        [[nodiscard]] auto get_gpu_to_cpu_offset_signed_ns() const noexcept -> int64_t
        {
            return _gpu_to_cpu_offset_ns;
        }

        [[nodiscard]] auto is_calibrated() const noexcept -> bool
        {
            return _is_calibrated;
        }

        [[nodiscard]] auto is_hardware_calibrated() const noexcept -> bool
        {
            return _is_hardware_calibrated;
        }

        [[nodiscard]] auto gpu_ticks_to_ns(uint64_t gpu_ticks) const noexcept -> uint64_t;

        [[nodiscard]] auto convert_gpu_timestamp_to_cpu_ns(uint64_t gpu_ticks) const noexcept -> uint64_t;

        static auto query_cpu_timestamp_ns() noexcept -> uint64_t;

      private:
        float _timestamp_period_ns;
        int64_t _gpu_to_cpu_offset_ns;
        bool _is_calibrated;
        bool _is_hardware_calibrated;
        VkTimeDomainEXT _host_time_domain;
    };
} // namespace tempest::rhi::vk

#endif // tempest_rhi_vk_calibration_hpp
