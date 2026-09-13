#include <gtest/gtest.h>

#include <tempest/atomic.hpp>
#include <tempest/job/job_system.hpp>
#include <tempest/job/when_all.hpp>
#include <tempest/logger.hpp>
#include <tempest/profiler/capture.hpp>
#include <tempest/profiler/session.hpp>
#include <tempest/string_view.hpp>
#include <tempest/vector.hpp>

namespace tempest::job::tests
{
    // =========================================================================
    // SECTION: Multi-Threaded Worker Execution & Parallel Throughput
    // =========================================================================

    /// @brief Verifies that a multi-threaded worker pool distributes and executes
    ///        asynchronous tasks across background threads until completion.
    TEST(worker_pool_test, multi_threaded_execution_stress)
    {
        // 1. Setup: Job system with 4 performance workers
        auto log = logger{};
        auto prof = profiler::profiler_session{false};
        auto config = job_system_config{
            .performance_worker_count = 4,
            .efficiency_worker_count = 0,
        };
        auto sys = job_system{log, prof, config};
        EXPECT_EQ(sys.worker_count(), 4u);

        constexpr auto task_count = 200;
        auto counter = atomic<int>{0};
        auto tasks = vector<task<void>>{};
        tasks.reserve(task_count);

        // 2. Act: Dispatch 200 tasks in parallel
        for (auto i = 0; i < task_count; ++i)
        {
            tasks.push_back(sys.async([&counter] {
                counter.fetch_add(1, memory_order::relaxed);
            }));
        }

        // Wait for all workers to complete active tasks
        sys.wait_idle();

        // 3. Assert: All 200 tasks ran to completion
        EXPECT_EQ(counter.load(memory_order::relaxed), task_count);
    }

    // =========================================================================
    // SECTION: Heterogeneous P/E Core Affinity Routing
    // =========================================================================

    /// @brief Verifies that tasks tagged with core_class::performance are routed
    ///        strictly to P-cores and core_class::efficiency to E-cores.
    TEST(worker_pool_test, core_affinity_routing)
    {
        // 1. Setup: Injected topology with 2 P-cores (cores 0, 1) and 2 E-cores (cores 2, 3)
        auto sim_topo = cpu_topology{};
        for (auto i = 0u; i < 2u; ++i)
        {
            sim_topo.cores.push_back(core_info{
                .core_id = i,
                .logical_core_index = i,
                .type = core_class::performance,
                .efficiency_class = 1,
                .affinity_mask = 1ULL << i,
            });
        }
        for (auto i = 2u; i < 4u; ++i)
        {
            sim_topo.cores.push_back(core_info{
                .core_id = i,
                .logical_core_index = i,
                .type = core_class::efficiency,
                .efficiency_class = 0,
                .affinity_mask = 1ULL << i,
            });
        }

        auto log = logger{};
        auto prof = profiler::profiler_session{false};
        auto config = job_system_config{
            .performance_worker_count = 2,
            .efficiency_worker_count = 2,
            .topology = sim_topo,
        };
        auto sys = job_system{log, prof, config};
        EXPECT_EQ(sys.performance_worker_count(), 2u);
        EXPECT_EQ(sys.efficiency_worker_count(), 2u);

        auto p_executed_count = atomic<int>{0};
        auto e_executed_count = atomic<int>{0};
        auto p_routed_correctly = atomic<bool>{true};
        auto e_routed_correctly = atomic<bool>{true};

        auto tasks = vector<task<void>>{};
        tasks.reserve(40);

        // 2. Act: Schedule 20 P-tasks and 20 E-tasks
        for (auto i = 0; i < 20; ++i)
        {
            tasks.push_back(sys.async(task_priority::normal, core_class::performance,
                                      [&sys, &p_executed_count, &p_routed_correctly] {
                                          auto cls = sys.current_worker_core_class();
                                          if (!cls.has_value() || *cls != core_class::performance)
                                          {
                                              p_routed_correctly.store(false, memory_order::relaxed);
                                          }
                                          p_executed_count.fetch_add(1, memory_order::relaxed);
                                      }));

            tasks.push_back(sys.async(task_priority::normal, core_class::efficiency,
                                      [&sys, &e_executed_count, &e_routed_correctly] {
                                          auto cls = sys.current_worker_core_class();
                                          if (!cls.has_value() || *cls != core_class::efficiency)
                                          {
                                              e_routed_correctly.store(false, memory_order::relaxed);
                                          }
                                          e_executed_count.fetch_add(1, memory_order::relaxed);
                                      }));
        }

        sys.wait_idle();

        // 3. Assert: Verify every task ran on its respective core class
        EXPECT_EQ(p_executed_count.load(memory_order::relaxed), 20);
        EXPECT_EQ(e_executed_count.load(memory_order::relaxed), 20);
        EXPECT_TRUE(p_routed_correctly.load(memory_order::relaxed));
        EXPECT_TRUE(e_routed_correctly.load(memory_order::relaxed));
    }

    // =========================================================================
    // SECTION: Cross-Core Stealing Invariants
    // =========================================================================

    /// @brief Verifies that idle E-core workers are strictly forbidden from stealing
    ///        tasks tagged with core_class::performance, even under high load.
    TEST(worker_pool_test, cross_core_stealing_rules)
    {
        // 1. Setup: 1 P-worker and 3 E-workers
        auto sim_topo = cpu_topology{};
        sim_topo.cores.push_back(core_info{
            .core_id = 0,
            .logical_core_index = 0,
            .type = core_class::performance,
            .efficiency_class = 1,
            .affinity_mask = 1ULL << 0,
        });
        for (auto i = 1u; i < 4u; ++i)
        {
            sim_topo.cores.push_back(core_info{
                .core_id = i,
                .logical_core_index = i,
                .type = core_class::efficiency,
                .efficiency_class = 0,
                .affinity_mask = 1ULL << i,
            });
        }

        auto log = logger{};
        auto prof = profiler::profiler_session{false};
        auto config = job_system_config{
            .performance_worker_count = 1,
            .efficiency_worker_count = 3,
            .enable_work_stealing = true,
            .topology = sim_topo,
        };
        auto sys = job_system{log, prof, config};

        auto perf_task_executed_by_p = atomic<int>{0};
        auto forbidden_theft_detected = atomic<bool>{false};

        constexpr auto heavy_task_count = 60;
        auto tasks = vector<task<void>>{};
        tasks.reserve(heavy_task_count);

        // 2. Act: Flood system with performance-only tasks
        for (auto i = 0; i < heavy_task_count; ++i)
        {
            tasks.push_back(sys.async(task_priority::normal, core_class::performance,
                                      [&sys, &perf_task_executed_by_p, &forbidden_theft_detected] {
                                          auto cls = sys.current_worker_core_class();
                                          if (cls.has_value() && *cls == core_class::efficiency)
                                          {
                                              forbidden_theft_detected.store(true, memory_order::relaxed);
                                          }
                                          else if (cls.has_value() && *cls == core_class::performance)
                                          {
                                              perf_task_executed_by_p.fetch_add(1, memory_order::relaxed);
                                          }
                                      }));
        }

        sys.wait_idle();

        // 3. Assert: 100% of performance tasks executed on P-core; zero thefts by E-cores
        EXPECT_EQ(perf_task_executed_by_p.load(memory_order::relaxed), heavy_task_count);
        EXPECT_FALSE(forbidden_theft_detected.load(memory_order::relaxed));
    }

    /// @brief Verify direct coroutine scheduling via job_context without ambient TLS or thread lookups.
    TEST(worker_pool_test, job_context_direct_schedule)
    {
        // 1. Setup: Multi-threaded worker pool with 2 workers
        auto log = logger{};
        auto prof = profiler::profiler_session{false};
        auto config = job_system_config{
            .performance_worker_count = 2,
            .efficiency_worker_count = 0,
            .enable_work_stealing = true,
        };
        auto sys = job_system{log, prof, config};

        auto executed = atomic<int>{0};

        // 2. Act: Construct an explicit job_context pointing to worker 0
        auto ctx = job_context{
            .system = sys,
            .allocator = sys.get_dispatch_allocator(),
            .worker_index = 0,
            .core_type = core_class::performance,
        };

        auto child_coroutine = [](atomic<int>& counter) -> task<void> {
            counter.fetch_add(1, memory_order::relaxed);
            co_return;
        };

        constexpr auto task_count = 20;
        auto tasks = vector<task<void>>{};
        tasks.reserve(task_count);

        for (auto i = 0; i < task_count; ++i)
        {
            auto t = child_coroutine(executed);
            ctx.schedule(t.handle());
            tasks.push_back(move(t));
        }

        sys.wait_idle();

        // 3. Assert: All coroutines scheduled through job_context executed to completion
        EXPECT_EQ(executed.load(memory_order::relaxed), task_count);
    }

    // =========================================================================
    // SECTION: Worker Thread & Coroutine Automatic Profiling Telemetry
    // =========================================================================

    /// @brief Verifies that tasks dispatched via async, parallel_for, and when_all
    ///        automatically record execution slices on worker threads without requiring
    ///        explicit with_profiler() calls, and that worker threads register valid tracks.
    TEST(worker_pool_test, worker_thread_and_coroutine_profiling_auto_binding)
    {
        // 1. Setup: Enable profiling session and job_system with performance and efficiency workers
        auto log = logger{};
        auto prof = profiler::profiler_session{true};
        auto config = job_system_config{
            .performance_worker_count = 2,
            .efficiency_worker_count = 2,
            .enable_work_stealing = true,
        };
        auto sys = job_system{log, prof, config};

        // 2. Act: Execute tasks via async, parallel_for, and when_all without manual with_profiler
        auto async_executed = atomic<int>{0};
        auto async_task = sys.async([&async_executed]() -> task<void> {
            async_executed.store(1, memory_order::release);
            co_return;
        });

        auto pfor_executed = atomic<int>{0};
        auto pfor_task = sys.parallel_for(range<size_t>{0, 20}, [&pfor_executed]([[maybe_unused]] size_t idx) {
            pfor_executed.fetch_add(1, memory_order::relaxed);
        });

        auto when_all_executed = atomic<int>{0};
        auto coro_factory = [&when_all_executed]() -> task<int> {
            when_all_executed.fetch_add(1, memory_order::relaxed);
            co_return 42;
        };

        auto leaf_tasks = vector<task<int>>{};
        for (auto i = 0; i < 4; ++i)
        {
            leaf_tasks.push_back(coro_factory());
        }
        auto all_task = when_all(sys, move(leaf_tasks));

        sys.schedule(pfor_task);
        sys.schedule(all_task);

        sys.wait_idle();

        EXPECT_EQ(async_executed.load(memory_order::acquire), 1);
        EXPECT_EQ(pfor_executed.load(memory_order::relaxed), 20);
        EXPECT_EQ(when_all_executed.load(memory_order::relaxed), 4);

        // 3. Assert: Capture profiler data and verify worker threads have recorded tracks and zones
        auto capture = profiler::create_capture_from_session(prof);

        auto found_worker_track = false;
        auto found_coroutine_slices = false;

        for (const auto& track : capture.tracks)
        {
            auto track_name_view = string_view{track.name.data(), track.name.size()};
            if (tempest::search(track_name_view, "JobWorker") != track_name_view.end())
            {
                found_worker_track = true;
                for (const auto& zone : track.zones)
                {
                    if (zone.coroutine_id > 0)
                    {
                        found_coroutine_slices = true;
                        break;
                    }
                }
            }
        }

        EXPECT_TRUE(found_worker_track);
        EXPECT_TRUE(found_coroutine_slices);
    }

    // =========================================================================
    // SECTION: High-Core-Count Server Topology (128 Cores / Multi-Group)
    // =========================================================================

    /// @brief Verifies that job_system correctly initializes worker pools on a 128-core
    ///        multi-group server topology, properly routing tasks across P-cores and E-cores.
    TEST(worker_pool_test, high_core_count_worker_routing_and_execution)
    {
        // 1. Setup: Synthesize 128-core topology (64 P-cores in Group 0 + 64 E-cores in Group 1)
        auto sim_topo = cpu_topology{};
        for (auto i = 0u; i < 64u; ++i)
        {
            sim_topo.cores.push_back(core_info{
                .core_id = i,
                .logical_core_index = i,
                .type = core_class::performance,
                .efficiency_class = 1,
                .processor_group = 0,
                .affinity_mask = 1ULL << i,
            });
        }
        for (auto i = 64u; i < 128u; ++i)
        {
            sim_topo.cores.push_back(core_info{
                .core_id = i,
                .logical_core_index = i,
                .type = core_class::efficiency,
                .efficiency_class = 0,
                .processor_group = 1,
                .affinity_mask = 1ULL << (i - 64u),
            });
        }

        auto log = logger{};
        auto prof = profiler::profiler_session{false};
        auto config = job_system_config{
            .performance_worker_count = 4,
            .efficiency_worker_count = 4,
            .enable_core_pinning = false, // Unit test host might have < 128 cores
            .topology = sim_topo,
        };

        auto sys = job_system{log, prof, config};
        EXPECT_EQ(sys.performance_worker_count(), 4u);
        EXPECT_EQ(sys.efficiency_worker_count(), 4u);

        // 2. Act: Dispatch mixed tasks across P-cores and E-cores
        auto p_executed_count = atomic<int>{0};
        auto e_executed_count = atomic<int>{0};
        constexpr auto task_batch = 50;
        auto tasks = vector<task<void>>{};
        tasks.reserve(task_batch * 2);

        for (auto i = 0; i < task_batch; ++i)
        {
            tasks.push_back(sys.async(task_priority::normal, core_class::performance,
                                      [&p_executed_count] {
                                          p_executed_count.fetch_add(1, memory_order::relaxed);
                                      }));

            tasks.push_back(sys.async(task_priority::normal, core_class::efficiency,
                                      [&e_executed_count] {
                                          e_executed_count.fetch_add(1, memory_order::relaxed);
                                      }));
        }

        sys.wait_idle();

        // 3. Assert: All 100 tasks on the 128-core simulated topology completed
        EXPECT_EQ(p_executed_count.load(memory_order::relaxed), task_batch);
        EXPECT_EQ(e_executed_count.load(memory_order::relaxed), task_batch);
    }
} // namespace tempest::job::tests
