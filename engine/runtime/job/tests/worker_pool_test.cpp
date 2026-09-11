#include <gtest/gtest.h>

#include <tempest/atomic.hpp>
#include <tempest/job/job_system.hpp>
#include <tempest/logger.hpp>
#include <tempest/profiler/session.hpp>
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
                                      [&p_executed_count, &p_routed_correctly] {
                                          auto cls = job_system::get_current_worker_core_class();
                                          if (!cls.has_value() || *cls != core_class::performance)
                                          {
                                              p_routed_correctly.store(false, memory_order::relaxed);
                                          }
                                          p_executed_count.fetch_add(1, memory_order::relaxed);
                                      }));

            tasks.push_back(sys.async(task_priority::normal, core_class::efficiency,
                                      [&e_executed_count, &e_routed_correctly] {
                                          auto cls = job_system::get_current_worker_core_class();
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
                                      [&perf_task_executed_by_p, &forbidden_theft_detected] {
                                          auto cls = job_system::get_current_worker_core_class();
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
} // namespace tempest::job::tests
