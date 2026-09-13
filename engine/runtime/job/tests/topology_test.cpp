#include <gtest/gtest.h>

#include <tempest/job/job_system.hpp>
#include <tempest/job/topology.hpp>
#include <tempest/logger.hpp>
#include <tempest/profiler/session.hpp>
#include <tempest/thread.hpp>

namespace tempest::job::tests
{
    // =========================================================================
    // SECTION: Hardware Topology Discovery
    // =========================================================================

    /// @brief Verifies that discover_cpu_topology() accurately discovers host CPU cores,
    ///        classifies at least one core as performance, and assigns valid affinity masks.
    TEST(topology_test, real_topology_discovery)
    {
        // 1. Setup & Act: Discover host hardware topology
        auto topo = discover_cpu_topology();

        // 2. Assert: Host must have at least one core and at least one performance core
        EXPECT_GT(topo.total_core_count(), 0u);
        EXPECT_GE(topo.performance_core_count(), 1u);
        EXPECT_EQ(topo.total_core_count(), topo.performance_core_count() + topo.efficiency_core_count());

        // Verify that masks and core indices are populated
        EXPECT_TRUE(topo.performance_mask().any());
        EXPECT_GT(topo.performance_mask().count(), 0u);
        for (auto i = 0u; i < topo.total_core_count(); ++i)
        {
            const auto& core = topo.cores[i];
            EXPECT_EQ(core.logical_core_index, i);
            EXPECT_EQ(core.affinity_mask, 1ULL << (core.logical_core_index % 64));
        }
    }

    // =========================================================================
    // SECTION: Simulated Topology Dependency Injection
    // =========================================================================

    /// @brief Verifies that job_system accepts an injected simulated cpu_topology,
    ///        correctly tracking simulated P-core and E-core counts and 1024-bit cpu_masks.
    TEST(topology_test, simulated_topology_injection)
    {
        // 1. Setup: Construct synthetic heterogeneous topology (4 P-cores + 4 E-cores)
        auto sim_topo = cpu_topology{};
        for (auto i = 0u; i < 4u; ++i)
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
        for (auto i = 4u; i < 8u; ++i)
        {
            sim_topo.cores.push_back(core_info{
                .core_id = i,
                .logical_core_index = i,
                .type = core_class::efficiency,
                .efficiency_class = 0,
                .processor_group = 0,
                .affinity_mask = 1ULL << i,
            });
        }

        auto log = logger{};
        auto prof = profiler::profiler_session{false};
        auto config = job_system_config{
            .performance_worker_count = 0,
            .efficiency_worker_count = 0,
            .topology = sim_topo,
        };

        // 2. Act: Initialize job system with injected topology
        auto sys = job_system{log, prof, config};

        // 3. Assert: Verify topology was retained and queried accurately
        const auto& loaded_topo = sys.get_topology();
        EXPECT_EQ(loaded_topo.total_core_count(), 8u);
        EXPECT_EQ(loaded_topo.performance_core_count(), 4u);
        EXPECT_EQ(loaded_topo.efficiency_core_count(), 4u);
        EXPECT_EQ(loaded_topo.performance_mask(), cpu_mask{0x0Fu});
        EXPECT_EQ(loaded_topo.efficiency_mask(), cpu_mask{0xF0u});
    }

    /// @brief Verifies that a high-core-count server topology (128 cores spanning 2 processor groups)
    ///        is properly represented in cpu_topology with accurate multi-group masks.
    TEST(topology_test, simulated_128_core_topology_injection)
    {
        // 1. Setup: Construct synthetic 128-core topology (64 P-cores in Group 0 + 64 E-cores in Group 1)
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
            .performance_worker_count = 0,
            .efficiency_worker_count = 0,
            .topology = sim_topo,
        };

        // 2. Act: Initialize job system with injected 128-core topology
        auto sys = job_system{log, prof, config};

        // 3. Assert: Verify 128-core topology queries across both groups
        const auto& loaded_topo = sys.get_topology();
        EXPECT_EQ(loaded_topo.total_core_count(), 128u);
        EXPECT_EQ(loaded_topo.performance_core_count(), 64u);
        EXPECT_EQ(loaded_topo.efficiency_core_count(), 64u);

        const auto perf_mask = loaded_topo.performance_mask();
        const auto eff_mask = loaded_topo.efficiency_mask();

        EXPECT_EQ(perf_mask.count(), 64u);
        EXPECT_EQ(eff_mask.count(), 64u);

        // Core 63 belongs to performance group 0
        EXPECT_TRUE(perf_mask.test(63));
        EXPECT_FALSE(perf_mask.test(64));

        // Core 64 and 127 belong to efficiency group 1
        EXPECT_FALSE(eff_mask.test(63));
        EXPECT_TRUE(eff_mask.test(64));
        EXPECT_TRUE(eff_mask.test(127));
    }

    // =========================================================================
    // SECTION: Thread Affinity Pinning
    // =========================================================================

    /// @brief Verifies thread affinity pinning helper sets affinity mask without error using core_info and cpu_mask.
    TEST(topology_test, thread_affinity_pinning)
    {
        // 1. Setup: Discover host topology and launch a background thread
        auto topo = discover_cpu_topology();
        auto executed = false;
        auto t = tempest::thread{[&topo, &executed] {
            auto self_pin_ok = set_current_thread_affinity(topo.cores[0]);
            EXPECT_TRUE(self_pin_ok);
            auto self_mask_ok = set_current_thread_affinity(topo.performance_mask());
            EXPECT_TRUE(self_mask_ok);
            executed = true;
        }};

        // 2. Act: Pin the background thread to the first core via core_info and cpu_mask
        auto pin_ok = set_thread_affinity(t, topo.cores[0]);
        auto mask_pin_ok = set_thread_affinity(t, topo.performance_mask());
        t.join();

        // 3. Assert: Both external and self pinning succeeded
        EXPECT_TRUE(pin_ok);
        EXPECT_TRUE(mask_pin_ok);
        EXPECT_TRUE(executed);
    }
} // namespace tempest::job::tests
