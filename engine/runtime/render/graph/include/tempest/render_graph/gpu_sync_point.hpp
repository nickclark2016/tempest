#ifndef tempest_render_graph_gpu_sync_point_hpp
#define tempest_render_graph_gpu_sync_point_hpp

#include <tempest/api.hpp>
#include <tempest/coroutine.hpp>
#include <tempest/expected.hpp>
#include <tempest/int.hpp>
#include <tempest/job/types.hpp>
#include <tempest/profiler/types.hpp>
#include <tempest/rhi.hpp>

namespace tempest::render_graph
{
    class gpu_timeline_monitor;

    enum class gpu_sync_error : uint8_t
    {
        none = 0,
        timeout,
        device_lost,
        cancelled,
        invalid_semaphore,
    };

    class TEMPEST_API gpu_sync_point
    {
      public:
        gpu_sync_point(gpu_timeline_monitor& monitor, rhi::host_sync_point sync_point,
                       job::task_priority priority = job::task_priority::normal,
                       job::core_class affinity = job::core_class::any) noexcept
            : _monitor{&monitor}, _sync_point{sync_point}, _priority{priority}, _affinity{affinity}
        {
        }

        [[nodiscard]] auto await_ready() noexcept -> bool;
        auto await_suspend(coroutine_handle<> handle) noexcept -> void;
        [[nodiscard]] auto await_resume() const noexcept -> expected<void, gpu_sync_error>;

        [[nodiscard]] constexpr auto suspend_reason_tag() const noexcept -> profiler::suspend_reason
        {
            return profiler::suspend_reason::timeline_wait;
        }

        [[nodiscard]] auto get_sync_point() const noexcept -> rhi::host_sync_point
        {
            return _sync_point;
        }

        [[nodiscard]] auto get_priority() const noexcept -> job::task_priority
        {
            return _priority;
        }

        [[nodiscard]] auto get_affinity() const noexcept -> job::core_class
        {
            return _affinity;
        }

      private:
        friend class gpu_timeline_monitor;

        gpu_timeline_monitor* _monitor{nullptr};
        rhi::host_sync_point _sync_point{};
        job::task_priority _priority{job::task_priority::normal};
        job::core_class _affinity{job::core_class::any};
        gpu_sync_error _error{gpu_sync_error::none};
    };
} // namespace tempest::render_graph

#endif // tempest_render_graph_gpu_sync_point_hpp
