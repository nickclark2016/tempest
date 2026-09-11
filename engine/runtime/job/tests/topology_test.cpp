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
        EXPECT_NE(topo.performance_mask(), 0u);
        for (auto i = 0u; i < topo.total_core_count(); ++i)
        {
            const auto& core = topo.cores[i];
            EXPECT_EQ(core.logical_core_index, i);
            EXPECT_NE(core.affinity_mask, 0u);
        }
    }

    // =========================================================================
    // SECTION: Simulated Topology Dependency Injection
    // =========================================================================

    /// @brief Verifies that job_system accepts an injected simulated cpu_topology,
    ///        correctly tracking simulated P-core and E-core counts.
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
        EXPECT_EQ(loaded_topo.performance_mask(), 0x0Fu);
        EXPECT_EQ(loaded_topo.efficiency_mask(), 0xF0u);
    }

    // =========================================================================
    // SECTION: Thread Affinity Pinning
    // =========================================================================

    /// @brief Verifies thread affinity pinning helper sets affinity mask without error.
    TEST(topology_test, thread_affinity_pinning)
    {
        // 1. Setup: Discover host topology and launch a background thread
        auto topo = discover_cpu_topology();
        auto executed = false;
        auto t = tempest::thread{[&executed] {
            auto self_pin_ok = set_current_thread_affinity(1ULL << 0);
            EXPECT_TRUE(self_pin_ok);
            executed = true;
        }};

        // 2. Act: Pin the background thread to the first core's affinity mask
        auto pin_ok = set_thread_affinity(t, topo.cores[0].affinity_mask);
        t.join();

        // 3. Assert: Both external and self pinning succeeded
        EXPECT_TRUE(pin_ok);
        EXPECT_TRUE(executed);
    }
} // namespace tempest::job::tests
