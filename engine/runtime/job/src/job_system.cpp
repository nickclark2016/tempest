#include <tempest/job/job_system.hpp>

namespace tempest::job
{
    namespace
    {
        thread_local job_system* tl_current_job_system = nullptr;

        class current_job_system_scope
        {
          public:
            explicit current_job_system_scope(job_system* sys) noexcept
                : _prev{tl_current_job_system}
            {
                tl_current_job_system = sys;
            }

            ~current_job_system_scope()
            {
                tl_current_job_system = _prev;
            }

            current_job_system_scope(const current_job_system_scope&) = delete;
            current_job_system_scope& operator=(const current_job_system_scope&) = delete;

          private:
            job_system* _prev{nullptr};
        };
    } // namespace

    job_system::job_system(logger& log, profiler::profiler_session& profiler, const job_system_config& config)
        : _logger{log}, _profiler{profiler}, _config{config}
    {
        _logger.info("Initializing tempest job_system in single-stepped mode");
    }

    job_system::~job_system()
    {
        _logger.info("Shutting down tempest job_system");
        wait_idle();
    }

    auto job_system::get_current() noexcept -> job_system*
    {
        return tl_current_job_system;
    }

    auto job_system::schedule(coroutine_handle<> handle, task_priority priority, core_class affinity) -> void
    {
        if (!handle || handle.done())
        {
            return;
        }

        auto prio_idx = static_cast<size_t>(priority);
        if (prio_idx >= static_cast<size_t>(task_priority::count))
        {
            prio_idx = static_cast<size_t>(task_priority::normal);
        }

        auto guard = lock_guard{_queue_mutex};
        _queues[prio_idx].push_back(queue_item{
            .handle = handle,
            .priority = priority,
            .affinity = affinity,
        });
    }

    auto job_system::step() -> bool
    {
        auto item = queue_item{};
        bool found = false;

        {
            auto guard = lock_guard{_queue_mutex};

            // Anti-starvation check: Every quantum steps of higher priority tasks, check low priority
            const auto low_idx = static_cast<size_t>(task_priority::low);
            if (_anti_starvation_counter >= _config.starvation_quantum && !_queues[low_idx].empty())
            {
                item = _queues[low_idx].front();
                _queues[low_idx].pop_front();
                _anti_starvation_counter = 0;
                found = true;
            }
            else
            {
                // Check priorities in strict descending order
                for (size_t p = 0; p < static_cast<size_t>(task_priority::count); ++p)
                {
                    if (!_queues[p].empty())
                    {
                        item = _queues[p].front();
                        _queues[p].pop_front();
                        found = true;
                        if (p != low_idx)
                        {
                            ++_anti_starvation_counter;
                        }
                        else
                        {
                            _anti_starvation_counter = 0;
                        }
                        break;
                    }
                }
            }
        }

        if (!found)
        {
            return false;
        }

        if (item.handle && !item.handle.done())
        {
            auto sys_scope = current_job_system_scope{this};
            auto alloc_scope = job_allocator_scope{_allocator};
            item.handle.resume();
        }

        return true;
    }

    auto job_system::step_for(size_t max_tasks) -> size_t
    {
        size_t executed = 0;
        while (executed < max_tasks && step())
        {
            ++executed;
        }
        return executed;
    }

    auto job_system::wait_idle() -> void
    {
        while (step())
        {
        }
    }
} // namespace tempest::job
