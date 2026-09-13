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
            syscall(SYS_futex, reinterpret_cast<int*>(addr), FUTEX_WAKE_PRIVATE, numeric_limits<int>::max(), nullptr,
                    nullptr, 0);
#elif defined(_WIN32)
            WakeByAddressAll(addr);
#endif
        }
    } // namespace

    struct worker_state
    {
        static constexpr size_t max_deque_size = 1024;

        job_system* owner = nullptr;
        size_t worker_index = 0U;
        core_class type{core_class::performance};
        uint64_t affinity_mask = 0ULL;
        array<work_stealing_deque<job_system::queue_item, max_deque_size>, static_cast<size_t>(task_priority::count)>
            deques{};
        concurrent_queue<job_system::queue_item> injection_queue;
        alignas(hardware_destructive_interference_size) atomic<uint32_t> park_state{0};
        uint32_t anti_starvation_counter = 0U;
        job_allocator allocator;
        tempest::thread worker_thread;
    };

    struct worker_thread_info
    {
        tempest::thread::id thread_id;
        worker_state* state = nullptr;
    };

    struct job_system::impl
    {
        cpu_topology topology{};
        bool is_single_stepped = false;
        uint32_t perf_worker_count = 0U;
        uint32_t eff_worker_count = 0U;

        // Multi-threaded worker pool
        vector<unique_ptr<worker_state>> workers;
        vector<worker_thread_info> worker_threads;
        atomic<bool> stop_requested = false;
        alignas(hardware_destructive_interference_size) atomic<uint64_t> idle_mask = 0ULL;
        atomic<size_t> next_injection_worker = 0U;
        atomic<int64_t> active_tasks = 0LL;

        // Single-stepped fallback mode
        mutable mutex single_stepped_mutex;
        array<deque<queue_item>, static_cast<size_t>(task_priority::count)> single_stepped_queues{};
        uint32_t single_stepped_anti_starvation = 0U;
        job_allocator single_stepped_allocator;
        job_allocator dispatch_allocator;

        [[nodiscard]] auto find_current_worker() const noexcept -> worker_state*
        {
            const auto self_id = this_thread::get_id();
            for (const auto& entry : worker_threads)
            {
                if (entry.thread_id == self_id)
                {
                    return entry.state;
                }
            }
            return nullptr;
        }

        auto wake_idle_worker() -> void
        {
            auto idle = idle_mask.load(memory_order::relaxed);
            if (idle != 0)
            {
                for (auto i = 0U; i < workers.size(); ++i)
                {
                    if ((idle & (1ULL << i)) != 0)
                    {
                        idle_mask.fetch_and(~(1ULL << i), memory_order::acq_rel);
                        workers[i]->park_state.store(0, memory_order::release);
                        futex_wake_one(&workers[i]->park_state);
                        break;
                    }
                }
            }
        }

        auto schedule_external(const queue_item& item, core_class affinity) -> void
        {
            auto target_idx = 0U;
            if (affinity == core_class::performance)
            {
                auto p_count = perf_worker_count;
                if (p_count > 0)
                {
                    auto next = next_injection_worker.fetch_add(1, memory_order::relaxed);
                    target_idx = next % p_count;
                }
            }
            else if (affinity == core_class::efficiency)
            {
                auto e_count = eff_worker_count;
                auto p_count = perf_worker_count;
                if (e_count > 0)
                {
                    auto next = next_injection_worker.fetch_add(1, memory_order::relaxed);
                    target_idx = p_count + (next % e_count);
                }
                else
                {
                    auto next = next_injection_worker.fetch_add(1, memory_order::relaxed);
                    target_idx = next % workers.size();
                }
            }
            else
            {
                auto next = next_injection_worker.fetch_add(1, memory_order::relaxed);
                target_idx = next % workers.size();
            }

            workers[target_idx]->injection_queue.push(item);
            idle_mask.fetch_and(~(1ULL << target_idx), memory_order::acq_rel);
            workers[target_idx]->park_state.store(0, memory_order::release);
            futex_wake_one(&workers[target_idx]->park_state);
        }
    };

    job_system::job_system(logger& log, profiler::profiler_session& profiler, job_system_config config)
        : _logger{log}, _profiler{profiler}, _config{tempest::move(config)}, _impl{make_unique<impl>()}
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
        auto core_idx = 0U;
        for (auto i = 0U; i < perf; ++i)
        {
            auto state = make_unique<worker_state>();
            state->owner = this;
            state->worker_index = _impl->workers.size();
            state->type = core_class::performance;

            // Find matching P-core in topology for pinning
            for (; core_idx < _impl->topology.cores.size(); ++core_idx)
            {
                if (_impl->topology.cores[core_idx].type == core_class::performance)
                {
                    state->affinity_mask = _impl->topology.cores[core_idx].affinity_mask;
                    ++core_idx;
                    break;
                }
            }

            for (auto priority = 0U; priority < static_cast<size_t>(task_priority::count); ++priority)
            {
                state->deques[priority].set_spill_queue(&state->injection_queue);
            }

            _impl->workers.push_back(tempest::move(state));
        }

        // Create efficiency workers
        core_idx = 0U;
        for (auto i = 0U; i < eff; ++i)
        {
            auto state = make_unique<worker_state>();
            state->owner = this;
            state->worker_index = _impl->workers.size();
            state->type = core_class::efficiency;

            // Find matching E-core in topology for pinning
            for (; core_idx < _impl->topology.cores.size(); ++core_idx)
            {
                if (_impl->topology.cores[core_idx].type == core_class::efficiency)
                {
                    state->affinity_mask = _impl->topology.cores[core_idx].affinity_mask;
                    ++core_idx;
                    break;
                }
            }

            for (auto priority = 0U; priority < static_cast<size_t>(task_priority::count); ++priority)
            {
                state->deques[priority].set_spill_queue(&state->injection_queue);
            }

            _impl->workers.push_back(tempest::move(state));
        }

        // Spawn worker threads
        for (auto& state : _impl->workers)
        {
            state->worker_thread = tempest::thread([this, worker = state.get()]() -> void {
                auto& prof_ctx = _profiler.get_or_register_thread();
                prof_ctx.set_thread_name(worker->type == core_class::performance ? "JobWorker-P" : "JobWorker-E");

                if (_config.enable_core_pinning && worker->affinity_mask != 0)
                {
                    set_current_thread_affinity(worker->affinity_mask);
                }

                auto pop_or_steal = [this](worker_state& work_state) -> optional<queue_item> {
                    const auto low_idx = static_cast<size_t>(task_priority::low);

                    // 1. Anti-starvation check
                    if (work_state.anti_starvation_counter >= _config.starvation_quantum)
                    {
                        auto item = work_state.deques[low_idx].pop();
                        if (item.has_value())
                        {
                            work_state.anti_starvation_counter = 0;
                            return item;
                        }
                    }

                    // 2. Local deques in priority order
                    for (auto priority = 0U; priority < static_cast<size_t>(task_priority::count); ++priority)
                    {
                        auto item = work_state.deques[priority].pop();
                        if (item.has_value())
                        {
                            if (priority != low_idx)
                            {
                                ++work_state.anti_starvation_counter;
                            }
                            else
                            {
                                work_state.anti_starvation_counter = 0;
                            }
                            return item;
                        }
                    }

                    // 3. Worker's own injection queue
                    auto inj_item = work_state.injection_queue.pop();
                    if (inj_item.has_value())
                    {
                        return inj_item;
                    }

                    // 4. Work stealing
                    if (!_config.enable_work_stealing || _impl->workers.size() <= 1)
                    {
                        return nullopt;
                    }

                    if (work_state.type == core_class::performance)
                    {
                        // P-cores steal from P-cores first (critical to low)
                        for (auto priority = 0U; priority < static_cast<size_t>(task_priority::count); ++priority)
                        {
                            for (auto worker_idx = 0U; worker_idx < _impl->workers.size(); ++worker_idx)
                            {
                                if (worker_idx == work_state.worker_index)
                                {
                                    continue;
                                }

                                auto& victim = *_impl->workers[worker_idx];
                                if (victim.type == core_class::performance)
                                {
                                    auto item = victim.deques[priority].steal();
                                    if (item.has_value())
                                    {
                                        return item;
                                    }
                                }
                            }
                        }

                        // P-cores steal any-tasks from E-cores
                        for (auto priority = 0U; priority < static_cast<size_t>(task_priority::count); ++priority)
                        {
                            for (auto worker_idx = 0U; worker_idx < _impl->workers.size(); ++worker_idx)
                            {
                                if (worker_idx == work_state.worker_index)
                                {
                                    continue;
                                }
                                auto& victim = *_impl->workers[worker_idx];
                                if (victim.type == core_class::efficiency)
                                {
                                    auto item = victim.deques[priority].steal_if(
                                        [](const queue_item& item) { return item.affinity == core_class::any; });
                                    if (item.has_value())
                                    {
                                        return item;
                                    }
                                }
                            }
                        }

                        // Check injection queues of other workers
                        for (auto worker_idx = 0U; worker_idx < _impl->workers.size(); ++worker_idx)
                        {
                            if (worker_idx == work_state.worker_index)
                            {
                                continue;
                            }
                            auto& victim = *_impl->workers[worker_idx];
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
                                auto item = victim.injection_queue.pop_if(
                                    [](const queue_item& item) -> bool { return item.affinity == core_class::any; });
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
                        for (auto priority = 0U; priority < static_cast<size_t>(task_priority::count); ++priority)
                        {
                            for (auto worker_idx = 0U; worker_idx < _impl->workers.size(); ++worker_idx)
                            {
                                if (worker_idx == work_state.worker_index)
                                {
                                    continue;
                                }
                                auto& victim = *_impl->workers[worker_idx];
                                if (victim.type == core_class::efficiency)
                                {
                                    auto item = victim.deques[priority].steal();
                                    if (item.has_value())
                                    {
                                        return item;
                                    }
                                }
                            }
                        }

                        // E-cores steal from P-cores when idle; FORBIDDEN from stealing performance tasks!
                        for (auto priority = 0U; priority < static_cast<size_t>(task_priority::count); ++priority)
                        {
                            for (auto worker_idx = 0U; worker_idx < _impl->workers.size(); ++worker_idx)
                            {
                                if (worker_idx == work_state.worker_index)
                                {
                                    continue;
                                }
                                auto& victim = *_impl->workers[worker_idx];
                                if (victim.type == core_class::performance)
                                {
                                    auto item = victim.deques[priority].steal_if([](const queue_item& item) -> bool {
                                        return item.affinity != core_class::performance;
                                    });
                                    if (item.has_value())
                                    {
                                        return item;
                                    }
                                }
                            }
                        }

                        // Check injection queues of other workers
                        for (auto worker_idx = 0U; worker_idx < _impl->workers.size(); ++worker_idx)
                        {
                            if (worker_idx == work_state.worker_index)
                            {
                                continue;
                            }
                            auto& victim = *_impl->workers[worker_idx];
                            auto item = victim.injection_queue.pop_if([](const queue_item& item) -> bool {
                                return item.affinity != core_class::performance;
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

                    constexpr auto spin_limit = 200;

                    // Phase 1: Bounded spin loop (~200 iterations)
                    auto found_work = false;
                    for (auto spin = 0; spin < spin_limit; ++spin)
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
            });
        }

        for (const auto& worker : _impl->workers)
        {
            _impl->worker_threads.push_back(worker_thread_info{
                .thread_id = worker->worker_thread.get_id(),
                .state = worker.get(),
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
            for (auto& worker : _impl->workers)
            {
                worker->park_state.store(0, memory_order::release);
                futex_wake_all(&worker->park_state);
            }

            for (auto& worker : _impl->workers)
            {
                if (worker->worker_thread.joinable())
                {
                    worker->worker_thread.join();
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
        auto* worker = _impl->find_current_worker();
        if (worker != nullptr && worker->owner == this)
        {
            return worker->type;
        }
        return nullopt;
    }

    auto job_system::current_worker_index() const noexcept -> optional<size_t>
    {
        auto* worker = _impl->find_current_worker();
        if (worker != nullptr && worker->owner == this)
        {
            return worker->worker_index;
        }
        return nullopt;
    }

    auto job_system::schedule(const job_context& ctx, coroutine_handle<> handle, task_priority priority,
                              core_class affinity) -> void
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

        if (ctx.worker_index < _impl->workers.size())
        {
            auto* curr_worker = _impl->workers[ctx.worker_index].get();
            auto compatible = (affinity == core_class::any) || (affinity == curr_worker->type);
            if (compatible)
            {
                if (this_thread::get_id() == curr_worker->worker_thread.get_id())
                {
                    curr_worker->deques[prio_idx].push(item);
                    _impl->wake_idle_worker();
                }
                else
                {
                    curr_worker->injection_queue.push(item);
                    _impl->idle_mask.fetch_and(~(1ULL << ctx.worker_index), memory_order::acq_rel);
                    curr_worker->park_state.store(0, memory_order::release);
                    futex_wake_one(&curr_worker->park_state);
                }
                return;
            }
        }

        _impl->schedule_external(item, affinity);
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

        auto* curr_worker = _impl->find_current_worker();
        if (curr_worker != nullptr && curr_worker->owner == this)
        {
            auto compatible = (affinity == core_class::any) || (affinity == curr_worker->type);
            if (compatible)
            {
                curr_worker->deques[prio_idx].push(item);
                _impl->wake_idle_worker();
                return;
            }
        }

        _impl->schedule_external(item, affinity);
    }

    auto job_system::step() -> bool
    {
        [[maybe_unused]] auto& prof_ctx = _profiler.get_or_register_thread();
        auto item = queue_item{};
        auto found = false;

        if (_impl->is_single_stepped || _impl->workers.empty())
        {
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
                    for (auto priority = 0U; priority < static_cast<size_t>(task_priority::count); ++priority)
                    {
                        if (!_impl->single_stepped_queues[priority].empty())
                        {
                            item = _impl->single_stepped_queues[priority].front();
                            _impl->single_stepped_queues[priority].pop_front();
                            found = true;
                            if (priority != low_idx)
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
            }

            if (!found)
            {
                return false;
            }

            if (item.handle && !item.handle.done())
            {
                item.handle.resume();
            }

            _impl->active_tasks.fetch_sub(1, memory_order::release);
            return true;
        }

        // Multi-threaded mode step: try popping from worker 0
        for (auto priority = 0U; priority < static_cast<size_t>(task_priority::count); ++priority)
        {
            auto res = _impl->workers[0]->deques[priority].pop();
            if (res.has_value())
            {
                if (res->handle && !res->handle.done())
                {
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
        auto executed = 0U;
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

    struct graph_executor
    {
        non_null<job_system> sys;
        atomic<size_t> remaining;
        atomic<uint8_t> first_error{static_cast<uint8_t>(job_error::none)};
        async_event completion_event;

        explicit graph_executor(job_system& sys, size_t count) : sys{sys}, remaining{count}, completion_event{sys}
        {
        }

        auto prune_node(task_node* node) -> void
        {
            node->_failed.store(true, memory_order::release);
            for (auto* succ : node->_successors)
            {
                succ->_failed.store(true, memory_order::release);
                if (succ->_runtime_in_degree.fetch_sub(1, memory_order::acq_rel) == 1)
                {
                    prune_node(succ);
                }
            }
            if (remaining.fetch_sub(1, memory_order::acq_rel) == 1)
            {
                completion_event.set();
            }
        }

        auto node_coro(task_node* node) -> task<void>
        {
            node->_result = co_await node->_invoker->execute();
            if (!node->_result.has_value())
            {
                auto expected_err = static_cast<uint8_t>(job_error::none);
                [[maybe_unused]] auto exchanged = first_error.compare_exchange_strong(
                    expected_err, static_cast<uint8_t>(node->_result.error()), memory_order::acq_rel);

                for (auto* succ : node->_successors)
                {
                    succ->_failed.store(true, memory_order::release);
                    if (succ->_runtime_in_degree.fetch_sub(1, memory_order::acq_rel) == 1)
                    {
                        prune_node(succ);
                    }
                }
            }
            else
            {
                for (auto* succ : node->_successors)
                {
                    if (succ->_runtime_in_degree.fetch_sub(1, memory_order::acq_rel) == 1)
                    {
                        if (succ->_failed.load(memory_order::acquire))
                        {
                            prune_node(succ);
                        }
                        else
                        {
                            schedule_node(succ);
                        }
                    }
                }
            }

            if (remaining.fetch_sub(1, memory_order::acq_rel) == 1)
            {
                completion_event.set();
            }
        }

        auto schedule_node(task_node* node) -> void
        {
            node->_task = node_coro(node);
            if (sys != nullptr)
            {
                node->_task.set_profiler(&sys->get_profiler());
                sys->schedule(node->_task.handle());
            }
        }

        auto init_and_run(task_graph& graph) -> void
        {
            for (auto& node : graph.nodes())
            {
                node->_runtime_in_degree.store(node->_static_in_degree, memory_order::relaxed);
                node->_failed.store(false, memory_order::relaxed);
                node->_result = expected<void, job_error>{};
                node->_task = task<void>{};
            }

            for (auto& node : graph.nodes())
            {
                if (node->_static_in_degree == 0)
                {
                    schedule_node(node.get());
                }
            }
        }
    };

    // SAFETY: The caller MUST guarantee that `graph` outlives the returned task.
    // This overload is intended for persistent subsystem-owned graphs.
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-reference-coroutine-parameters)
    auto job_system::execute(task_graph& graph) -> task<expected<void, error_code>>
    {
        if (graph.empty())
        {
            co_return expected<void, error_code>{};
        }

        auto executor = make_unique<graph_executor>(*this, graph.size());
        executor->init_and_run(graph);

        co_await executor->completion_event.wait();

        auto final_err = static_cast<job_error>(executor->first_error.load(memory_order::acquire));
        if (final_err != job_error::none)
        {
            co_await unexpected{final_err};
        }

        co_return expected<void, error_code>{};
    }

    auto job_system::execute(task_graph&& graph) -> task<task_graph_result, void>
    {
        return [](job_system* sys, task_graph target) -> task<task_graph_result, void> {
            if (target.empty())
            {
                co_return task_graph_result{
                    .graph = tempest::move(target),
                    .status = expected<void, error_code>{},
                };
            }

            auto executor = make_unique<graph_executor>(*sys, target.size());
            executor->init_and_run(target);

            co_await executor->completion_event.wait();

            auto final_err = static_cast<job_error>(executor->first_error.load(memory_order::acquire));
            if (final_err != job_error::none)
            {
                co_return task_graph_result{
                    .graph = tempest::move(target),
                    .status = unexpected{final_err},
                };
            }

            co_return task_graph_result{
                .graph = tempest::move(target),
                .status = expected<void, error_code>{},
            };
        }(this, tempest::move(graph));
    }

    auto job_system::get_dispatch_allocator() noexcept -> job_allocator&
    {
        auto* worker = _impl->find_current_worker();
        if (worker != nullptr && worker->owner == this)
        {
            return worker->allocator;
        }
        return _impl->dispatch_allocator;
    }

    auto job_system::allocate_frame(size_t size) -> void*
    {
        return get_dispatch_allocator().allocate(size);
    }
} // namespace tempest::job
