#ifndef tempest_render_graph_gpu_timeline_monitor_hpp
#define tempest_render_graph_gpu_timeline_monitor_hpp

#include <tempest/api.hpp>
#include <tempest/atomic.hpp>
#include <tempest/coroutine.hpp>
#include <tempest/int.hpp>
#include <tempest/intrusive_stack.hpp>
#include <tempest/job/job_system.hpp>
#include <tempest/logger.hpp>
#include <tempest/render_graph/gpu_sync_point.hpp>
#include <tempest/rhi.hpp>
#include <tempest/thread.hpp>
#include <tempest/vector.hpp>

namespace tempest::render_graph
{
    class TEMPEST_API gpu_timeline_monitor
    {
      public:
        using wait_entry = gpu_wait_entry;

        gpu_timeline_monitor(rhi::device& dev, job::job_system& jobs, logger& log);
        ~gpu_timeline_monitor();

        gpu_timeline_monitor(const gpu_timeline_monitor&) = delete;
        gpu_timeline_monitor& operator=(const gpu_timeline_monitor&) = delete;
        gpu_timeline_monitor(gpu_timeline_monitor&&) noexcept = delete;
        gpu_timeline_monitor& operator=(gpu_timeline_monitor&&) noexcept = delete;

        [[nodiscard]] auto wait(rhi::host_sync_point sync_point,
                                job::task_priority priority = job::task_priority::normal,
                                job::core_class affinity = job::core_class::any) -> gpu_sync_point;

        auto stop() -> void;

        [[nodiscard]] auto get_device() const noexcept -> rhi::device&
        {
            return _device;
        }

        [[nodiscard]] auto get_job_system() const noexcept -> job::job_system&
        {
            return _jobs;
        }

        [[nodiscard]] auto get_logger() const noexcept -> logger&
        {
            return _log;
        }

        auto register_wait(wait_entry& entry) -> void;
        auto wake() noexcept -> void;
        [[nodiscard]] auto is_stopped() const noexcept -> bool;

      private:
        auto _monitor_loop() -> void;

        rhi::device& _device;
        job::job_system& _jobs;
        logger& _log;

        rhi::semaphore_handle _control_semaphore{};
        atomic<uint64_t> _control_seq{0};
        uint64_t _last_seen_control_val{0};

        atomic<bool> _stop_requested{false};
        thread _thread{};

        intrusive_mpsc_stack<wait_entry> _pending_requests{};
        vector<wait_entry*> _active_entries{};
    };
} // namespace tempest::render_graph

#endif // tempest_render_graph_gpu_timeline_monitor_hpp
