#include <tempest/render_graph/gpu_timeline_monitor.hpp>
#include <tempest/span.hpp>

namespace tempest::render_graph
{
    auto gpu_sync_point::await_ready() noexcept -> bool
    {
        if (_sync_point.semaphore == rhi::semaphore_handle{})
        {
            _error = gpu_sync_error::invalid_semaphore;
            return true;
        }

        const auto current_val = _monitor->get_device().get_semaphore_value(_sync_point.semaphore);
        if (current_val >= _sync_point.value)
        {
            _error = gpu_sync_error::none;
            return true;
        }
        return false;
    }

    auto gpu_sync_point::await_suspend(coroutine_handle<> handle) noexcept -> void
    {
        _monitor->register_wait(handle, _sync_point, _priority, _affinity, &_error);
    }

    auto gpu_sync_point::await_resume() const noexcept -> expected<void, gpu_sync_error>
    {
        if (_error != gpu_sync_error::none)
        {
            return unexpected(_error);
        }
        return {};
    }

    gpu_timeline_monitor::gpu_timeline_monitor(rhi::device& dev, job::job_system& jobs, logger& log)
        : _device{dev}, _jobs{jobs}, _log{log}
    {
        _control_semaphore = _device.create_timeline_semaphore();
        _thread = thread{[this]() { _monitor_loop(); }};
    }

    gpu_timeline_monitor::~gpu_timeline_monitor()
    {
        stop();
        if (_control_semaphore != rhi::semaphore_handle{})
        {
            _device.destroy_semaphore(_control_semaphore);
            _control_semaphore = {};
        }
    }

    auto gpu_timeline_monitor::wait(rhi::host_sync_point sync_point,
                                    job::task_priority priority,
                                    job::core_class affinity) -> gpu_sync_point
    {
        return gpu_sync_point{*this, sync_point, priority, affinity};
    }

    auto gpu_timeline_monitor::register_wait(coroutine_handle<> handle, rhi::host_sync_point sp,
                                             job::task_priority priority, job::core_class affinity,
                                             gpu_sync_error* error_out) -> void
    {
        if (_stop_requested.load(memory_order::relaxed))
        {
            if (error_out != nullptr)
            {
                *error_out = gpu_sync_error::cancelled;
            }
            _jobs.schedule(handle, priority, affinity);
            return;
        }

        {
            auto guard = lock_guard{_mutex};
            _pending_requests.push_back(wait_entry{
                .handle = handle,
                .sync_point = sp,
                .priority = priority,
                .affinity = affinity,
                .error_out = error_out,
            });
        }

        const auto next_seq = _control_seq.fetch_add(1, memory_order::release) + 1;
        _device.signal_semaphore(_control_semaphore, next_seq);
    }

    auto gpu_timeline_monitor::stop() -> void
    {
        if (_stop_requested.exchange(true, memory_order::acq_rel))
        {
            return;
        }

        const auto next_seq = _control_seq.fetch_add(1, memory_order::release) + 1;
        _device.signal_semaphore(_control_semaphore, next_seq);

        if (_thread.joinable())
        {
            _thread.join();
        }

        auto guard = lock_guard{_mutex};
        for (auto& entry : _pending_requests)
        {
            if (entry.error_out != nullptr)
            {
                *entry.error_out = gpu_sync_error::cancelled;
            }
            _jobs.schedule(entry.handle, entry.priority, entry.affinity);
        }
        _pending_requests.clear();

        for (auto& entry : _active_entries)
        {
            if (entry.error_out != nullptr)
            {
                *entry.error_out = gpu_sync_error::cancelled;
            }
            _jobs.schedule(entry.handle, entry.priority, entry.affinity);
        }
        _active_entries.clear();
    }

    auto gpu_timeline_monitor::_monitor_loop() -> void
    {
        while (!_stop_requested.load(memory_order::acquire))
        {
            // 1. Drain pending requests
            {
                auto guard = lock_guard{_mutex};
                for (auto& req : _pending_requests)
                {
                    _active_entries.push_back(req);
                }
                _pending_requests.clear();
            }

            // 2. Check completions
            auto remaining_entries = vector<wait_entry>{};
            remaining_entries.reserve(_active_entries.size());

            for (auto& entry : _active_entries)
            {
                const auto current_val = _device.get_semaphore_value(entry.sync_point.semaphore);
                if (current_val >= entry.sync_point.value)
                {
                    if (entry.error_out != nullptr)
                    {
                        *entry.error_out = gpu_sync_error::none;
                    }
                    _jobs.schedule(entry.handle, entry.priority, entry.affinity);
                }
                else
                {
                    remaining_entries.push_back(entry);
                }
            }
            _active_entries = tempest::move(remaining_entries);

            if (_stop_requested.load(memory_order::acquire))
            {
                break;
            }

            // 3. Build unique sync points
            _last_seen_control_val = _device.get_semaphore_value(_control_semaphore);

            auto has_pending = false;
            {
                auto guard = lock_guard{_mutex};
                has_pending = !_pending_requests.empty();
            }
            if (has_pending)
            {
                continue;
            }

            auto wait_points = vector<rhi::host_sync_point>{};
            wait_points.push_back(rhi::host_sync_point{
                .semaphore = _control_semaphore,
                .value = _last_seen_control_val + 1,
            });

            for (const auto& entry : _active_entries)
            {
                auto found = false;
                for (auto& wp : wait_points)
                {
                    if (wp.semaphore == entry.sync_point.semaphore)
                    {
                        found = true;
                        if (entry.sync_point.value < wp.value)
                        {
                            wp.value = entry.sync_point.value;
                        }
                        break;
                    }
                }
                if (!found)
                {
                    wait_points.push_back(entry.sync_point);
                }
            }

            // 4. Wait
            constexpr auto wait_timeout_ns = uint64_t{50'000'000ULL}; // 50 ms
            const auto status = _device.wait_semaphores(
                span<const rhi::host_sync_point>{wait_points.data(), wait_points.size()},
                wait_timeout_ns,
                /*wait_any=*/true
            );

            if (status == rhi::wait_status::device_lost)
            {
                _log.error("GPU device lost while monitoring timeline semaphores");
                auto guard = lock_guard{_mutex};
                for (auto& entry : _pending_requests)
                {
                    if (entry.error_out != nullptr)
                    {
                        *entry.error_out = gpu_sync_error::device_lost;
                    }
                    _jobs.schedule(entry.handle, entry.priority, entry.affinity);
                }
                _pending_requests.clear();

                for (auto& entry : _active_entries)
                {
                    if (entry.error_out != nullptr)
                    {
                        *entry.error_out = gpu_sync_error::device_lost;
                    }
                    _jobs.schedule(entry.handle, entry.priority, entry.affinity);
                }
                _active_entries.clear();
                break;
            }
        }
    }
} // namespace tempest::render_graph
