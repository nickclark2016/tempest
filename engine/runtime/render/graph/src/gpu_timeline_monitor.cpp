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

    gpu_sync_point::~gpu_sync_point() noexcept
    {
        if (_suspended)
        {
            auto expected_state = wait_state::pending;
            if (_entry.state.compare_exchange_strong(expected_state, wait_state::cancelled, memory_order::acq_rel))
            {
                if (!_monitor->is_stopped())
                {
                    _monitor->wake();
                    while (_entry.state.load(memory_order::acquire) != wait_state::retired && !_monitor->is_stopped())
                    {
                        tempest::this_thread::yield();
                    }
                }
            }
        }
    }

    gpu_sync_point::gpu_sync_point(gpu_sync_point&& other) noexcept
        : _monitor{other._monitor}
        , _sync_point{other._sync_point}
        , _priority{other._priority}
        , _affinity{other._affinity}
        , _error{other._error}
        , _suspended{other._suspended}
    {
        TEMPEST_ASSERT(!other._suspended);
        other._suspended = false;
    }

    auto gpu_sync_point::operator=(gpu_sync_point&& other) noexcept -> gpu_sync_point&
    {
        if (this != &other)
        {
            TEMPEST_ASSERT(!_suspended && !other._suspended);
            _monitor = other._monitor;
            _sync_point = other._sync_point;
            _priority = other._priority;
            _affinity = other._affinity;
            _error = other._error;
            _suspended = other._suspended;
            other._suspended = false;
        }
        return *this;
    }

    auto gpu_sync_point::await_suspend(coroutine_handle<> handle) noexcept -> void
    {
        _suspended = true;
        _entry.handle = handle;
        _entry.sync_point = _sync_point;
        _entry.priority = _priority;
        _entry.affinity = _affinity;
        _entry.error_out = &_error;
        _entry.next = nullptr;
        _entry.state.store(wait_state::pending, memory_order::relaxed);
        _monitor->register_wait(_entry);
    }

    auto gpu_sync_point::await_resume() noexcept -> expected<void, gpu_sync_error>
    {
        _suspended = false;
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

    auto gpu_timeline_monitor::wake() noexcept -> void
    {
        const auto next_seq = _control_seq.fetch_add(1, memory_order::release) + 1;
        _device.signal_semaphore(_control_semaphore, next_seq);
    }

    auto gpu_timeline_monitor::is_stopped() const noexcept -> bool
    {
        return _stop_requested.load(memory_order::acquire);
    }

    auto gpu_timeline_monitor::register_wait(wait_entry& entry) -> void
    {
        if (_stop_requested.load(memory_order::acquire))
        {
            entry.state.store(wait_state::completed, memory_order::release);
            if (entry.error_out != nullptr)
            {
                *entry.error_out = gpu_sync_error::cancelled;
            }
            _jobs.schedule(entry.handle, entry.priority, entry.affinity);
            return;
        }

        _pending_requests.push(entry);

        wake();
    }

    auto gpu_timeline_monitor::stop() -> void
    {
        if (_stop_requested.exchange(true, memory_order::acq_rel))
        {
            return;
        }

        wake();

        if (_thread.joinable())
        {
            _thread.join();
        }

        auto* pending_head = _pending_requests.drain();
        while (pending_head != nullptr)
        {
            auto* const next_node = intrusive_mpsc_stack<wait_entry>::next(pending_head);
            auto expected_state = wait_state::pending;
            if (pending_head->state.compare_exchange_strong(expected_state, wait_state::completed, memory_order::acq_rel))
            {
                if (pending_head->error_out != nullptr)
                {
                    *pending_head->error_out = gpu_sync_error::cancelled;
                }
                _jobs.schedule(pending_head->handle, pending_head->priority, pending_head->affinity);
            }
            else if (expected_state == wait_state::cancelled)
            {
                pending_head->state.store(wait_state::retired, memory_order::release);
            }
            pending_head = next_node;
        }

        for (auto* entry : _active_entries)
        {
            auto expected_state = wait_state::pending;
            if (entry->state.compare_exchange_strong(expected_state, wait_state::completed, memory_order::acq_rel))
            {
                if (entry->error_out != nullptr)
                {
                    *entry->error_out = gpu_sync_error::cancelled;
                }
                _jobs.schedule(entry->handle, entry->priority, entry->affinity);
            }
            else if (expected_state == wait_state::cancelled)
            {
                entry->state.store(wait_state::retired, memory_order::release);
            }
        }
        _active_entries.clear();
    }

    auto gpu_timeline_monitor::_monitor_loop() -> void
    {
        while (!_stop_requested.load(memory_order::acquire))
        {
            // 1. Drain pending requests
            auto* pending_head = _pending_requests.drain();
            while (pending_head != nullptr)
            {
                auto* const next_node = intrusive_mpsc_stack<wait_entry>::next(pending_head);
                pending_head->next = nullptr;
                if (pending_head->state.load(memory_order::acquire) == wait_state::cancelled)
                {
                    pending_head->state.store(wait_state::retired, memory_order::release);
                }
                else
                {
                    _active_entries.push_back(pending_head);
                }
                pending_head = next_node;
            }

            // 2. Check completions
            auto remaining_entries = vector<wait_entry*>{};
            remaining_entries.reserve(_active_entries.size());

            for (auto* entry : _active_entries)
            {
                if (entry->state.load(memory_order::acquire) == wait_state::cancelled)
                {
                    entry->state.store(wait_state::retired, memory_order::release);
                    continue;
                }

                const auto current_val = _device.get_semaphore_value(entry->sync_point.semaphore);
                if (current_val >= entry->sync_point.value)
                {
                    auto expected_state = wait_state::pending;
                    if (entry->state.compare_exchange_strong(expected_state, wait_state::completed, memory_order::acq_rel))
                    {
                        if (entry->error_out != nullptr)
                        {
                            *entry->error_out = gpu_sync_error::none;
                        }
                        _jobs.schedule(entry->handle, entry->priority, entry->affinity);
                    }
                    else if (expected_state == wait_state::cancelled)
                    {
                        entry->state.store(wait_state::retired, memory_order::release);
                    }
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

            if (!_pending_requests.empty())
            {
                continue;
            }

            auto wait_points = vector<rhi::host_sync_point>{};
            wait_points.push_back(rhi::host_sync_point{
                .semaphore = _control_semaphore,
                .value = _last_seen_control_val + 1,
            });

            for (const auto* entry : _active_entries)
            {
                auto found = false;
                for (auto& wp : wait_points)
                {
                    if (wp.semaphore == entry->sync_point.semaphore)
                    {
                        found = true;
                        if (entry->sync_point.value < wp.value)
                        {
                            wp.value = entry->sync_point.value;
                        }
                        break;
                    }
                }
                if (!found)
                {
                    wait_points.push_back(entry->sync_point);
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
                auto* lost_pending = _pending_requests.drain();
                while (lost_pending != nullptr)
                {
                    auto* const next_node = intrusive_mpsc_stack<wait_entry>::next(lost_pending);
                    auto expected_state = wait_state::pending;
                    if (lost_pending->state.compare_exchange_strong(expected_state, wait_state::completed, memory_order::acq_rel))
                    {
                        if (lost_pending->error_out != nullptr)
                        {
                            *lost_pending->error_out = gpu_sync_error::device_lost;
                        }
                        _jobs.schedule(lost_pending->handle, lost_pending->priority, lost_pending->affinity);
                    }
                    else if (expected_state == wait_state::cancelled)
                    {
                        lost_pending->state.store(wait_state::retired, memory_order::release);
                    }
                    lost_pending = next_node;
                }

                for (auto* entry : _active_entries)
                {
                    auto expected_state = wait_state::pending;
                    if (entry->state.compare_exchange_strong(expected_state, wait_state::completed, memory_order::acq_rel))
                    {
                        if (entry->error_out != nullptr)
                        {
                            *entry->error_out = gpu_sync_error::device_lost;
                        }
                        _jobs.schedule(entry->handle, entry->priority, entry->affinity);
                    }
                    else if (expected_state == wait_state::cancelled)
                    {
                        entry->state.store(wait_state::retired, memory_order::release);
                    }
                }
                _active_entries.clear();
                break;
            }
        }
    }
} // namespace tempest::render_graph
