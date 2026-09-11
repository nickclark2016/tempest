#include <tempest/job/job_system.hpp>

#include <tempest/algorithm.hpp>
#include <tempest/array.hpp>
#include <tempest/atomic.hpp>
#include <tempest/deque.hpp>
#include <tempest/job/work_stealing_deque.hpp>
#include <tempest/mutex.hpp>
#include <tempest/thread.hpp>
#include <tempest/vector.hpp>

#if defined(__linux__)
#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>
#elif defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace tempest::job
{
    namespace
    {
        inline auto cpu_pause() noexcept -> void
        {
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
            asm volatile("pause" ::: "memory");
#elif defined(__aarch64__) || defined(_M_ARM64)
            asm volatile("yield" ::: "memory");
#else
            this_thread::yield();
#endif
        }

        inline auto futex_wait(atomic<uint32_t>* addr, uint32_t val) -> void
        {
#if defined(__linux__)
            syscall(SYS_futex, reinterpret_cast<int*>(addr), FUTEX_WAIT_PRIVATE, val, nullptr, nullptr, 0);
#elif defined(_WIN32)
            WaitOnAddress(addr, &val, sizeof(uint32_t), INFINITE);
#else
            this_thread::sleep_for_nanoseconds(50'000);
#endif
        }

        inline auto futex_wake_one(atomic<uint32_t>* addr) -> void
        {
#if defined(__linux__)
            syscall(SYS_futex, reinterpret_cast<int*>(addr), FUTEX_WAKE_PRIVATE, 1, nullptr, nullptr, 0);
#elif defined(_WIN32)
            WakeByAddressSingle(addr);
#endif
        }

        inline auto futex_wake_all(atomic<uint32_t>* addr) -> void
        {
#if defined(__linux__)
            syscall(SYS_futex, reinterpret_cast<int*>(addr), FUTEX_WAKE_PRIVATE, 2147483647, nullptr, nullptr, 0);
#elif defined(_WIN32)
            WakeByAddressAll(addr);
#endif
        }
    } // namespace

    struct worker_state
    {
        job_system* owner{nullptr};
        size_t worker_index{0};
        core_class type{core_class::performance};
        uint64_t affinity_mask{0};
        array<work_stealing_deque<job_system::queue_item, 1024>, static_cast<size_t>(task_priority::count)> deques{};
        concurrent_queue<job_system::queue_item> injection_queue{};
        alignas(64) atomic<uint32_t> park_state{0};
        uint32_t anti_starvation_counter{0};
        job_allocator allocator{};
        tempest::thread worker_thread{};
    };

    namespace
    {
        thread_local worker_state* tl_current_worker = nullptr;
    }

    struct job_system::impl
    {
        cpu_topology topology{};
        bool is_single_stepped{false};
        uint32_t perf_worker_count{0};
        uint32_t eff_worker_count{0};

        // Multi-threaded worker pool
        vector<unique_ptr<worker_state>> workers{};
        atomic<bool> stop_requested{false};
        alignas(64) atomic<uint64_t> idle_mask{0};
        atomic<size_t> next_injection_worker{0};
        atomic<int64_t> active_tasks{0};

        // Single-stepped fallback mode
        mutable mutex single_stepped_mutex{};
        array<deque<queue_item>, static_cast<size_t>(task_priority::count)> single_stepped_queues{};
        uint32_t single_stepped_anti_starvation{0};
        job_allocator single_stepped_allocator{};
    };

    job_system::job_system(logger& log, profiler::profiler_session& profiler, const job_system_config& config)
        : _logger{log}, _profiler{profiler}, _config{config}, _impl{make_unique<impl>()}
    {
        // 1. Determine topology
        if (_config.topology.has_value())
        {
            _impl->topology = *_config.topology;
        }
        else
        {
            _impl->topology = discover_cpu_topology();
        }

        // 2. Determine worker counts
        auto explicitly_zero = false;
        if (_config.performance_worker_count.has_value() && *_config.performance_worker_count == 0 &&
            _config.efficiency_worker_count.has_value() && *_config.efficiency_worker_count == 0)
        {
            explicitly_zero = true;
        }

        if (explicitly_zero)
        {
            _impl->is_single_stepped = true;
            _impl->perf_worker_count = 0;
            _impl->eff_worker_count = 0;
            _logger.info("Initializing tempest job_system in single-stepped mode (0 workers)");
            return;
        }

        auto perf = _config.performance_worker_count.value_or(_impl->topology.performance_core_count());
        auto eff = _config.efficiency_worker_count.value_or(_impl->topology.efficiency_core_count());

        if (perf == 0 && eff == 0)
        {
            perf = 1;
        }

        _impl->perf_worker_count = perf;
        _impl->eff_worker_count = eff;
        _impl->is_single_stepped = false;

        auto total_workers = perf + eff;
        _logger.info("Initializing tempest job_system with worker pool");

        _impl->workers.reserve(total_workers);

        // Create performance workers
        auto core_idx = 0u;
        for (auto i = 0u; i < perf; ++i)
        {
            auto w = make_unique<worker_state>();
            w->owner = this;
            w->worker_index = _impl->workers.size();
            w->type = core_class::performance;

            // Find matching P-core in topology for pinning
            for (; core_idx < _impl->topology.cores.size(); ++core_idx)
            {
                if (_impl->topology.cores[core_idx].type == core_class::performance)
                {
                    w->affinity_mask = _impl->topology.cores[core_idx].affinity_mask;
                    ++core_idx;
                    break;
                }
            }

            for (auto p = 0u; p < static_cast<size_t>(task_priority::count); ++p)
            {
                w->deques[p].set_spill_queue(&w->injection_queue);
            }

            _impl->workers.push_back(tempest::move(w));
        }

        // Create efficiency workers
        core_idx = 0u;
        for (auto i = 0u; i < eff; ++i)
        {
            auto w = make_unique<worker_state>();
            w->owner = this;
            w->worker_index = _impl->workers.size();
            w->type = core_class::efficiency;

            // Find matching E-core in topology for pinning
            for (; core_idx < _impl->topology.cores.size(); ++core_idx)
            {
                if (_impl->topology.cores[core_idx].type == core_class::efficiency)
                {
                    w->affinity_mask = _impl->topology.cores[core_idx].affinity_mask;
                    ++core_idx;
                    break;
                }
            }

            for (auto p = 0u; p < static_cast<size_t>(task_priority::count); ++p)
            {
                w->deques[p].set_spill_queue(&w->injection_queue);
            }

            _impl->workers.push_back(tempest::move(w));
        }

        // Spawn worker threads
        for (auto& w : _impl->workers)
        {
            w->worker_thread = tempest::thread([this, worker = w.get()] {
                auto alloc_scope = job_allocator_scope{worker->allocator};
                tl_current_worker = worker;

                if (_config.enable_core_pinning && worker->affinity_mask != 0)
                {
                    set_current_thread_affinity(worker->affinity_mask);
                }

                auto pop_or_steal = [this](worker_state& ws) -> optional<queue_item> {
                    const auto low_idx = static_cast<size_t>(task_priority::low);

                    // 1. Anti-starvation check
                    if (ws.anti_starvation_counter >= _config.starvation_quantum)
                    {
                        auto item = ws.deques[low_idx].pop();
                        if (item.has_value())
                        {
                            ws.anti_starvation_counter = 0;
                            return item;
                        }
                    }

                    // 2. Local deques in priority order
                    for (auto p = 0u; p < static_cast<size_t>(task_priority::count); ++p)
                    {
                        auto item = ws.deques[p].pop();
                        if (item.has_value())
                        {
                            if (p != low_idx)
                            {
                                ++ws.anti_starvation_counter;
                            }
                            else
                            {
                                ws.anti_starvation_counter = 0;
                            }
                            return item;
                        }
                    }

                    // 3. Worker's own injection queue
                    auto inj_item = ws.injection_queue.pop();
                    if (inj_item.has_value())
                    {
                        return inj_item;
                    }

                    // 4. Work stealing
                    if (!_config.enable_work_stealing || _impl->workers.size() <= 1)
                    {
                        return nullopt;
                    }

                    if (ws.type == core_class::performance)
                    {
                        // P-cores steal from P-cores first (critical to low)
                        for (auto p = 0u; p < static_cast<size_t>(task_priority::count); ++p)
                        {
                            for (auto i = 0u; i < _impl->workers.size(); ++i)
                            {
                                if (i == ws.worker_index)
                                {
                                    continue;
                                }
                                auto& victim = *_impl->workers[i];
                                if (victim.type == core_class::performance)
                                {
                                    auto item = victim.deques[p].steal();
                                    if (item.has_value())
                                    {
                                        return item;
                                    }
                                }
                            }
                        }

                        // P-cores steal any-tasks from E-cores
                        for (auto p = 0u; p < static_cast<size_t>(task_priority::count); ++p)
                        {
                            for (auto i = 0u; i < _impl->workers.size(); ++i)
                            {
                                if (i == ws.worker_index)
                                {
                                    continue;
                                }
                                auto& victim = *_impl->workers[i];
                                if (victim.type == core_class::efficiency)
                                {
                                    auto item = victim.deques[p].steal_if([](const queue_item& q) {
                                        return q.affinity == core_class::any;
                                    });
                                    if (item.has_value())
                                    {
                                        return item;
                                    }
                                }
                            }
                        }

                        // Check injection queues of other workers
                        for (auto i = 0u; i < _impl->workers.size(); ++i)
                        {
                            if (i == ws.worker_index)
                            {
                                continue;
                            }
                            auto& victim = *_impl->workers[i];
                            if (victim.type == core_class::performance)
                            {
                                auto item = victim.injection_queue.pop();
                                if (item.has_value())
                                {
                                    return item;
                                }
                            }
                            else
                            {
                                auto item = victim.injection_queue.pop_if([](const queue_item& q) {
                                    return q.affinity == core_class::any;
                                });
                                if (item.has_value())
                                {
                                    return item;
                                }
                            }
                        }
                    }
                    else // Efficiency core
                    {
                        // E-cores steal from E-cores first
                        for (auto p = 0u; p < static_cast<size_t>(task_priority::count); ++p)
                        {
                            for (auto i = 0u; i < _impl->workers.size(); ++i)
                            {
                                if (i == ws.worker_index)
                                {
                                    continue;
                                }
                                auto& victim = *_impl->workers[i];
                                if (victim.type == core_class::efficiency)
                                {
                                    auto item = victim.deques[p].steal();
                                    if (item.has_value())
                                    {
                                        return item;
                                    }
                                }
                            }
                        }

                        // E-cores steal from P-cores when idle; FORBIDDEN from stealing performance tasks!
                        for (auto p = 0u; p < static_cast<size_t>(task_priority::count); ++p)
                        {
                            for (auto i = 0u; i < _impl->workers.size(); ++i)
                            {
                                if (i == ws.worker_index)
                                {
                                    continue;
                                }
                                auto& victim = *_impl->workers[i];
                                if (victim.type == core_class::performance)
                                {
                                    auto item = victim.deques[p].steal_if([](const queue_item& q) {
                                        return q.affinity != core_class::performance;
                                    });
                                    if (item.has_value())
                                    {
                                        return item;
                                    }
                                }
                            }
                        }

                        // Check injection queues of other workers
                        for (auto i = 0u; i < _impl->workers.size(); ++i)
                        {
                            if (i == ws.worker_index)
                            {
                                continue;
                            }
                            auto& victim = *_impl->workers[i];
                            auto item = victim.injection_queue.pop_if([](const queue_item& q) {
                                return q.affinity != core_class::performance;
                            });
                            if (item.has_value())
                            {
                                return item;
                            }
                        }
                    }

                    return nullopt;
                };

                // Worker main execution loop
                while (!_impl->stop_requested.load(memory_order::relaxed))
                {
                    auto item = pop_or_steal(*worker);
                    if (item.has_value())
                    {
                        if (item->handle && !item->handle.done())
                        {
                            item->handle.resume();
                        }
                        _impl->active_tasks.fetch_sub(1, memory_order::release);
                        continue;
                    }

                    // Phase 1: Bounded spin loop (~200 iterations)
                    auto found_work = false;
                    for (auto spin = 0; spin < 200; ++spin)
                    {
                        cpu_pause();
                        item = pop_or_steal(*worker);
                        if (item.has_value())
                        {
                            found_work = true;
                            if (item->handle && !item->handle.done())
                            {
                                item->handle.resume();
                            }
                            _impl->active_tasks.fetch_sub(1, memory_order::release);
                            break;
                        }
                    }

                    if (found_work)
                    {
                        continue;
                    }

                    // Phase 2: OS Park
                    _impl->idle_mask.fetch_or(1ULL << worker->worker_index, memory_order::acq_rel);
                    worker->park_state.store(1, memory_order::release);

                    // Recheck work before parking
                    item = pop_or_steal(*worker);
                    if (item.has_value())
                    {
                        _impl->idle_mask.fetch_and(~(1ULL << worker->worker_index), memory_order::acq_rel);
                        worker->park_state.store(0, memory_order::release);
                        if (item->handle && !item->handle.done())
                        {
                            item->handle.resume();
                        }
                        _impl->active_tasks.fetch_sub(1, memory_order::release);
                        continue;
                    }

                    if (!_impl->stop_requested.load(memory_order::relaxed))
                    {
                        futex_wait(&worker->park_state, 1);
                    }

                    _impl->idle_mask.fetch_and(~(1ULL << worker->worker_index), memory_order::acq_rel);
                    worker->park_state.store(0, memory_order::release);
                }

                tl_current_worker = nullptr;
            });
        }
    }

    job_system::~job_system()
    {
        _logger.info("Shutting down tempest job_system");
        wait_idle();

        if (!_impl->is_single_stepped)
        {
            _impl->stop_requested.store(true, memory_order::release);
            for (auto& w : _impl->workers)
            {
                w->park_state.store(0, memory_order::release);
                futex_wake_all(&w->park_state);
            }

            for (auto& w : _impl->workers)
            {
                if (w->worker_thread.joinable())
                {
                    w->worker_thread.join();
                }
            }
        }
    }

    auto job_system::get_topology() const noexcept -> const cpu_topology&
    {
        return _impl->topology;
    }

    auto job_system::worker_count() const noexcept -> uint32_t
    {
        return static_cast<uint32_t>(_impl->workers.size());
    }

    auto job_system::performance_worker_count() const noexcept -> uint32_t
    {
        return _impl->perf_worker_count;
    }

    auto job_system::efficiency_worker_count() const noexcept -> uint32_t
    {
        return _impl->eff_worker_count;
    }

    auto job_system::current_worker_core_class() const noexcept -> optional<core_class>
    {
        if (tl_current_worker != nullptr && tl_current_worker->owner == this)
        {
            return tl_current_worker->type;
        }
        return nullopt;
    }

    auto job_system::current_worker_index() const noexcept -> optional<size_t>
    {
        if (tl_current_worker != nullptr && tl_current_worker->owner == this)
        {
            return tl_current_worker->worker_index;
        }
        return nullopt;
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

        auto item = queue_item{
            .handle = handle,
            .priority = priority,
            .affinity = affinity,
        };

        _impl->active_tasks.fetch_add(1, memory_order::relaxed);

        if (_impl->is_single_stepped || _impl->workers.empty())
        {
            auto guard = lock_guard{_impl->single_stepped_mutex};
            _impl->single_stepped_queues[prio_idx].push_back(item);
            return;
        }

        auto* curr_worker = tl_current_worker;
        if (curr_worker != nullptr && curr_worker->owner == this)
        {
            // Worker thread scheduling
            auto compatible = (affinity == core_class::any) || (affinity == curr_worker->type);
            if (compatible)
            {
                curr_worker->deques[prio_idx].push(item);

                // Wake an idle worker to steal if available
                auto idle = _impl->idle_mask.load(memory_order::relaxed);
                if (idle != 0)
                {
                    for (auto i = 0u; i < _impl->workers.size(); ++i)
                    {
                        if (idle & (1ULL << i))
                        {
                            _impl->workers[i]->park_state.store(0, memory_order::release);
                            futex_wake_one(&_impl->workers[i]->park_state);
                            break;
                        }
                    }
                }
                return;
            }
        }

        // External thread or cross-affinity scheduling
        auto target_idx = 0u;
        if (affinity == core_class::performance)
        {
            auto p_count = _impl->perf_worker_count;
            if (p_count > 0)
            {
                auto next = _impl->next_injection_worker.fetch_add(1, memory_order::relaxed);
                target_idx = next % p_count;
            }
        }
        else if (affinity == core_class::efficiency)
        {
            auto e_count = _impl->eff_worker_count;
            auto p_count = _impl->perf_worker_count;
            if (e_count > 0)
            {
                auto next = _impl->next_injection_worker.fetch_add(1, memory_order::relaxed);
                target_idx = p_count + (next % e_count);
            }
            else
            {
                // Fallback to P-core if no E-cores
                auto next = _impl->next_injection_worker.fetch_add(1, memory_order::relaxed);
                target_idx = next % _impl->workers.size();
            }
        }
        else
        {
            auto next = _impl->next_injection_worker.fetch_add(1, memory_order::relaxed);
            target_idx = next % _impl->workers.size();
        }

        _impl->workers[target_idx]->injection_queue.push(item);
        _impl->workers[target_idx]->park_state.store(0, memory_order::release);
        futex_wake_one(&_impl->workers[target_idx]->park_state);
    }

    auto job_system::step() -> bool
    {
        auto item = queue_item{};
        auto found = false;

        if (_impl->is_single_stepped || _impl->workers.empty())
        {
            auto guard = lock_guard{_impl->single_stepped_mutex};

            const auto low_idx = static_cast<size_t>(task_priority::low);
            if (_impl->single_stepped_anti_starvation >= _config.starvation_quantum &&
                !_impl->single_stepped_queues[low_idx].empty())
            {
                item = _impl->single_stepped_queues[low_idx].front();
                _impl->single_stepped_queues[low_idx].pop_front();
                _impl->single_stepped_anti_starvation = 0;
                found = true;
            }
            else
            {
                for (auto p = 0u; p < static_cast<size_t>(task_priority::count); ++p)
                {
                    if (!_impl->single_stepped_queues[p].empty())
                    {
                        item = _impl->single_stepped_queues[p].front();
                        _impl->single_stepped_queues[p].pop_front();
                        found = true;
                        if (p != low_idx)
                        {
                            ++_impl->single_stepped_anti_starvation;
                        }
                        else
                        {
                            _impl->single_stepped_anti_starvation = 0;
                        }
                        break;
                    }
                }
            }

            if (!found)
            {
                return false;
            }

            if (item.handle && !item.handle.done())
            {
                auto alloc_scope = job_allocator_scope{_impl->single_stepped_allocator};
                item.handle.resume();
            }

            _impl->active_tasks.fetch_sub(1, memory_order::release);
            return true;
        }

        // Multi-threaded mode step: try popping from worker 0
        for (auto p = 0u; p < static_cast<size_t>(task_priority::count); ++p)
        {
            auto res = _impl->workers[0]->deques[p].pop();
            if (res.has_value())
            {
                if (res->handle && !res->handle.done())
                {
                    auto alloc_scope = job_allocator_scope{_impl->workers[0]->allocator};
                    res->handle.resume();
                }
                _impl->active_tasks.fetch_sub(1, memory_order::release);
                return true;
            }
        }

        return false;
    }

    auto job_system::step_for(size_t max_tasks) -> size_t
    {
        auto executed = 0u;
        while (executed < max_tasks && step())
        {
            ++executed;
        }
        return executed;
    }

    auto job_system::wait_idle() -> void
    {
        if (_impl->is_single_stepped || _impl->workers.empty())
        {
            while (step())
            {
            }
            return;
        }

        while (_impl->active_tasks.load(memory_order::acquire) > 0)
        {
            cpu_pause();
            this_thread::yield();
        }
    }

    auto job_system::execute(task_graph& graph) -> task<expected<void, error_code>>
    {
        if (graph.empty())
        {
            co_return expected<void, error_code>{};
        }

        auto count = graph.size();
        auto remaining = make_unique<atomic<size_t>>(count);
        auto first_error = make_unique<atomic<uint8_t>>(static_cast<uint8_t>(job_error::none));
        auto completion_event = make_unique<async_event>(*this);
        auto node_tasks = make_unique<vector<task<void>>>();
        auto tasks_mutex = make_unique<mutex>();

        // 1. Reset runtime counters with zero allocations
        for (auto& n : graph.nodes())
        {
            n->_runtime_in_degree.store(n->_static_in_degree, memory_order::relaxed);
            n->_failed.store(false, memory_order::relaxed);
            n->_result = expected<void, job_error>{};
        }

        // Helper to prune downstream nodes when an ancestor fails
        auto prune_node = [&remaining, &completion_event](auto& self, task_node* n) -> void {
            n->_failed.store(true, memory_order::release);
            for (auto* succ : n->_successors)
            {
                succ->_failed.store(true, memory_order::release);
                if (succ->_runtime_in_degree.fetch_sub(1, memory_order::acq_rel) == 1)
                {
                    self(self, succ);
                }
            }
            if (remaining->fetch_sub(1, memory_order::acq_rel) == 1)
            {
                completion_event->set();
            }
        };

        // Helper to dispatch a node
        auto schedule_node = [this, rem = remaining.get(), err = first_error.get(), ev = completion_event.get(),
                              tasks = node_tasks.get(), t_mutex = tasks_mutex.get(),
                              &prune_node](auto& self, task_node* n) -> void {
            auto t = async(task_priority::normal, core_class::any,
                           [this, n, rem, err, ev, &prune_node, &self]() -> task<void> {
                               co_await n->_invoker->execute(*n);
                               if (!n->_result.has_value())
                               {
                                   auto expected_err = static_cast<uint8_t>(job_error::none);
                                   [[maybe_unused]] auto exchanged =
                                       err->compare_exchange_strong(expected_err, static_cast<uint8_t>(n->_result.error()),
                                                                    memory_order::acq_rel);

                                   for (auto* succ : n->_successors)
                                   {
                                       succ->_failed.store(true, memory_order::release);
                                       if (succ->_runtime_in_degree.fetch_sub(1, memory_order::acq_rel) == 1)
                                       {
                                           prune_node(prune_node, succ);
                                       }
                                   }
                               }
                               else
                               {
                                   for (auto* succ : n->_successors)
                                   {
                                       if (succ->_runtime_in_degree.fetch_sub(1, memory_order::acq_rel) == 1)
                                       {
                                           if (succ->_failed.load(memory_order::acquire))
                                           {
                                               prune_node(prune_node, succ);
                                           }
                                           else
                                           {
                                               self(self, succ);
                                           }
                                       }
                                   }
                               }

                               if (rem->fetch_sub(1, memory_order::acq_rel) == 1)
                               {
                                   ev->set();
                               }
                           });

            auto guard = lock_guard{*t_mutex};
            tasks->push_back(tempest::move(t));
        };

        // Find and schedule root nodes
        for (auto& n : graph.nodes())
        {
            if (n->_static_in_degree == 0)
            {
                schedule_node(schedule_node, n.get());
            }
        }

        co_await completion_event->wait();

        auto final_err = static_cast<job_error>(first_error->load(memory_order::acquire));
        if (final_err != job_error::none)
        {
            co_await unexpected{final_err};
        }

        co_return expected<void, error_code>{};
    }
} // namespace tempest::job
