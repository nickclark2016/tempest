#ifndef tempest_render_graph_gpu_sync_point_hpp
#define tempest_render_graph_gpu_sync_point_hpp

#include <tempest/api.hpp>
#include <tempest/checked.hpp>
#include <tempest/coroutine.hpp>
#include <tempest/expected.hpp>
#include <tempest/int.hpp>
#include <tempest/intrusive_stack.hpp>
#include <tempest/job/types.hpp>
#include <tempest/profiler/types.hpp>
#include <tempest/rhi.hpp>

#include <tempest/assert.hpp>
#include <tempest/atomic.hpp>

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

    enum class wait_state : uint8_t
    {
        pending = 0,
        completed,
        cancelled,
        retired,
    };

    struct gpu_wait_entry : treiber_node<gpu_wait_entry>
    {
        coroutine_handle<> handle{};
        rhi::host_sync_point sync_point{};
        job::task_priority priority{job::task_priority::normal};
        job::core_class affinity{job::core_class::any};
        gpu_sync_error* error_out{nullptr};
        atomic<wait_state> state{wait_state::pending};
    };

    class TEMPEST_API gpu_sync_point
    {
      public:
        gpu_sync_point(gpu_timeline_monitor& monitor, rhi::host_sync_point sync_point,
                       job::task_priority priority = job::task_priority::normal,
                       job::core_class affinity = job::core_class::any) noexcept
            : _monitor{monitor}, _sync_point{sync_point}, _priority{priority}, _affinity{affinity}
        {
        }

        ~gpu_sync_point() noexcept;

        gpu_sync_point(const gpu_sync_point&) = delete;
        auto operator=(const gpu_sync_point&) -> gpu_sync_point& = delete;

        gpu_sync_point(gpu_sync_point&& other) noexcept;
        auto operator=(gpu_sync_point&& other) noexcept -> gpu_sync_point&;

        [[nodiscard]] auto await_ready() noexcept -> bool;
        auto await_suspend(coroutine_handle<> handle) noexcept -> bool;
        [[nodiscard]] auto await_resume() noexcept -> expected<void, gpu_sync_error>;

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

        non_null<gpu_timeline_monitor> _monitor;
        rhi::host_sync_point _sync_point{};
        job::task_priority _priority{job::task_priority::normal};
        job::core_class _affinity{job::core_class::any};
        gpu_sync_error _error{gpu_sync_error::none};
        bool _suspended{false};
        gpu_wait_entry _entry{};
    };
} // namespace tempest::render_graph

#endif // tempest_render_graph_gpu_sync_point_hpp
