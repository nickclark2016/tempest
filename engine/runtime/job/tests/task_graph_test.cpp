#include <gtest/gtest.h>

#include <tempest/atomic.hpp>
#include <tempest/checked.hpp>
#include <tempest/job/job_system.hpp>
#include <tempest/job/task_graph.hpp>
#include <tempest/logger.hpp>
#include <tempest/profiler/session.hpp>
#include <tempest/span.hpp>
#include <tempest/vector.hpp>

namespace tempest::job::tests
{
    // =========================================================================
    // SECTION: Fan-Out / Fan-In Dependency Reduction
    // =========================================================================

    /// @brief Verifies DAG topological execution order: 1 -> N (fan-out) and N -> 1 (fan-in),
    ///        ensuring downstream nodes only execute after all predecessors complete.
    TEST(task_graph_test, fan_out_fan_in_dependency_reduction)
    {
        // 1. Setup: 4-worker job system and task graph
        auto log = logger{};
        auto prof = profiler::profiler_session{false};
        auto config = job_system_config{
            .performance_worker_count = 4,
            .efficiency_worker_count = 0,
        };
        auto sys = job_system{log, prof, config};

        auto graph = task_graph{};

        auto cull_done = atomic<bool>{false};
        auto shadow_done = atomic<bool>{false};
        auto depth_done = atomic<bool>{false};
        auto light_done = atomic<bool>{false};

        auto shadow_saw_cull = atomic<bool>{false};
        auto depth_saw_cull = atomic<bool>{false};
        auto light_saw_all = atomic<bool>{false};

        auto& cull = graph.emplace("Cull", [&] {
            cull_done.store(true, memory_order::release);
        });

        auto& shadow = graph.emplace("Shadows", [&] {
            if (cull_done.load(memory_order::acquire))
            {
                shadow_saw_cull.store(true, memory_order::release);
            }
            shadow_done.store(true, memory_order::release);
        });

        auto& depth = graph.emplace("Depth", [&] {
            if (cull_done.load(memory_order::acquire))
            {
                depth_saw_cull.store(true, memory_order::release);
            }
            depth_done.store(true, memory_order::release);
        });

        auto& light = graph.emplace("Light", [&] {
            if (shadow_done.load(memory_order::acquire) && depth_done.load(memory_order::acquire))
            {
                light_saw_all.store(true, memory_order::release);
            }
            light_done.store(true, memory_order::release);
        });

        // 2. Act: Wire dependencies
        cull.precede(shadow, depth);
        light.succeed(shadow, depth);

        EXPECT_EQ(cull.in_degree(), 0u);
        EXPECT_EQ(cull.out_degree(), 2u);
        EXPECT_EQ(light.in_degree(), 2u);
        EXPECT_EQ(light.out_degree(), 0u);

        // Execute graph
        auto run_task = [&sys, &graph]() -> task<expected<void, error_code>> {
            co_return co_await sys.execute(graph);
        };
        auto exec = run_task();
        exec.resume();
        sys.wait_idle();

        // 3. Assert: All dependency reduction invariants held
        ASSERT_TRUE(exec.is_ready());
        EXPECT_TRUE(exec.value().has_value());
        EXPECT_TRUE(cull_done.load(memory_order::relaxed));
        EXPECT_TRUE(shadow_done.load(memory_order::relaxed));
        EXPECT_TRUE(depth_done.load(memory_order::relaxed));
        EXPECT_TRUE(light_done.load(memory_order::relaxed));
        EXPECT_TRUE(shadow_saw_cull.load(memory_order::relaxed));
        EXPECT_TRUE(depth_saw_cull.load(memory_order::relaxed));
        EXPECT_TRUE(light_saw_all.load(memory_order::relaxed));
    }

    // =========================================================================
    // SECTION: Zero-Allocation Reusable Frame Execution
    // =========================================================================

    /// @brief Verifies that a task_graph DAG can be repeatedly executed across frames
    ///        with zero memory reallocation, correctly resetting runtime atomic counters.
    TEST(task_graph_test, reusable_frame_execution)
    {
        // 1. Setup: 4-worker job system
        auto log = logger{};
        auto prof = profiler::profiler_session{false};
        auto config = job_system_config{
            .performance_worker_count = 4,
            .efficiency_worker_count = 0,
        };
        auto sys = job_system{log, prof, config};

        auto graph = task_graph{};
        auto iteration_count = atomic<int>{0};

        auto& n1 = graph.emplace("Node1", [&iteration_count] {
            iteration_count.fetch_add(1, memory_order::relaxed);
        });
        auto& n2 = graph.emplace("Node2", [&iteration_count] {
            iteration_count.fetch_add(1, memory_order::relaxed);
        });
        auto& n3 = graph.emplace("Node3", [&iteration_count] {
            iteration_count.fetch_add(1, memory_order::relaxed);
        });

        n1.precede(n2);
        n2.precede(n3);

        // 2. Act: Execute 5 frames consecutively without modifying the graph
        for (auto frame = 0; frame < 5; ++frame)
        {
            auto run_frame = [&sys, &graph]() -> task<expected<void, error_code>> {
                co_return co_await sys.execute(graph);
            };
            auto t = run_frame();
            t.resume();
            sys.wait_idle();

            ASSERT_TRUE(t.is_ready());
            EXPECT_TRUE(t.value().has_value());
        }

        // 3. Assert: 3 nodes executed exactly 5 times = 15 invocations
        EXPECT_EQ(iteration_count.load(memory_order::relaxed), 15);
    }

    // =========================================================================
    // SECTION: Subtree Error Pruning
    // =========================================================================

    /// @brief Verifies that when a node fails, its downstream dependents are pruned
    ///        and never executed, while independent branches complete normally.
    TEST(task_graph_test, subtree_error_pruning)
    {
        // 1. Setup:
        // Branch 1: Root1 -> FailedNode -> Child1 (should be skipped!)
        // Branch 2: Root2 -> IndependentChild (should complete normally!)
        auto log = logger{};
        auto prof = profiler::profiler_session{false};
        auto config = job_system_config{
            .performance_worker_count = 4,
            .efficiency_worker_count = 0,
        };
        auto sys = job_system{log, prof, config};

        auto graph = task_graph{};

        auto root1_executed = atomic<bool>{false};
        auto failed_node_executed = atomic<bool>{false};
        auto child1_executed = atomic<bool>{false};

        auto root2_executed = atomic<bool>{false};
        auto independent_child_executed = atomic<bool>{false};

        auto& root1 = graph.emplace("Root1", [&] {
            root1_executed.store(true, memory_order::release);
        });

        auto& failed_node = graph.emplace("FailedNode", [&]() -> expected<void, job_error> {
            failed_node_executed.store(true, memory_order::release);
            return unexpected{job_error::task_failed};
        });

        auto& child1 = graph.emplace("Child1", [&] {
            child1_executed.store(true, memory_order::release);
        });

        auto& root2 = graph.emplace("Root2", [&] {
            root2_executed.store(true, memory_order::release);
        });

        auto& independent_child = graph.emplace("IndependentChild", [&] {
            independent_child_executed.store(true, memory_order::release);
        });

        // 2. Act: Wire dependencies
        root1.precede(failed_node);
        failed_node.precede(child1);

        root2.precede(independent_child);

        auto run = [&sys, &graph]() -> task<expected<void, error_code>> {
            co_return co_await sys.execute(graph);
        };
        auto t = run();
        t.resume();
        sys.wait_idle();

        // 3. Assert:
        // - Root1 and FailedNode ran
        // - Child1 was PRUNED (never executed!)
        // - Independent Branch 2 ran completely
        // - Graph returned the error
        ASSERT_TRUE(t.is_ready());
        EXPECT_FALSE(t.has_value());
        EXPECT_EQ(t.error(), job_error::task_failed);

        EXPECT_TRUE(root1_executed.load(memory_order::relaxed));
        EXPECT_TRUE(failed_node_executed.load(memory_order::relaxed));
        EXPECT_FALSE(child1_executed.load(memory_order::relaxed));

        EXPECT_TRUE(root2_executed.load(memory_order::relaxed));
        EXPECT_TRUE(independent_child_executed.load(memory_order::relaxed));
    }

    // =========================================================================
    // SECTION: Coroutine-Native Graph Nodes
    // =========================================================================

    /// @brief Verifies that graph nodes can be asynchronous coroutines returning task<T, E>.
    TEST(task_graph_test, coroutine_native_nodes)
    {
        // 1. Setup: 4-worker job system
        auto log = logger{};
        auto prof = profiler::profiler_session{false};
        auto config = job_system_config{
            .performance_worker_count = 4,
            .efficiency_worker_count = 0,
        };
        auto sys = job_system{log, prof, config};

        auto graph = task_graph{};

        auto step1_val = atomic<int>{0};
        auto step2_val = atomic<int>{0};

        // 2. Act: Emplace coroutine nodes
        auto& coro1 = graph.emplace("Coro1", [&step1_val]() -> task<void> {
            step1_val.store(42, memory_order::release);
            co_return;
        });

        auto& coro2 = graph.emplace("Coro2", [&step1_val, &step2_val]() -> task<void> {
            auto v1 = step1_val.load(memory_order::acquire);
            step2_val.store(v1 * 2, memory_order::release);
            co_return;
        });

        coro1.precede(coro2);

        auto run = [&sys, &graph]() -> task<expected<void, error_code>> {
            co_return co_await sys.execute(graph);
        };
        auto t = run();
        t.resume();
        sys.wait_idle();

        // 3. Assert: Both coroutine nodes executed in order
        ASSERT_TRUE(t.is_ready());
        EXPECT_TRUE(t.value().has_value());
        EXPECT_EQ(step1_val.load(memory_order::relaxed), 42);
        EXPECT_EQ(step2_val.load(memory_order::relaxed), 84);
    }

    /// @brief Verifies that task graph dependencies can be wired via span<const non_null<task_node>>.
    TEST(task_graph_test, non_null_span_dependencies)
    {
        // 1. Setup
        auto log = logger{};
        auto prof = profiler::profiler_session{false};
        auto sys = job_system{log, prof};

        auto graph = task_graph{};
        auto a_done = atomic<bool>{false};
        auto b_saw_a = atomic<bool>{false};
        auto c_saw_a = atomic<bool>{false};

        auto& node_a = graph.emplace("A", [&] { a_done.store(true, memory_order::release); });
        auto& node_b = graph.emplace("B", [&] {
            if (a_done.load(memory_order::acquire))
            {
                b_saw_a.store(true, memory_order::release);
            }
        });
        auto& node_c = graph.emplace("C", [&] {
            if (a_done.load(memory_order::acquire))
            {
                c_saw_a.store(true, memory_order::release);
            }
        });

        // 2. Act: Precede via span<const non_null<task_node>>
        auto deps = vector<non_null<task_node>>{};
        deps.push_back(node_b);
        deps.push_back(node_c);
        node_a.precede(span<const non_null<task_node>>{deps.data(), deps.size()});

        auto run = [&sys, &graph]() -> task<expected<void, error_code>> {
            co_return co_await sys.execute(graph);
        };
        auto t = run();
        t.resume();
        sys.wait_idle();

        // 3. Assert
        ASSERT_TRUE(t.is_ready());
        EXPECT_TRUE(t.value().has_value());
        EXPECT_TRUE(b_saw_a.load(memory_order::acquire));
        EXPECT_TRUE(c_saw_a.load(memory_order::acquire));
    }

    // =========================================================================
    // SECTION: Hybrid Linear Ownership Execution (By-Value)
    // =========================================================================

    /// @brief Verifies that passing a task_graph by value/move executes all tasks,
    ///        returns the intact graph inside task_graph_result, and unpacks via structured bindings.
    TEST(task_graph_test, by_value_execution_success)
    {
        // 1. Setup: 4-worker job system and task graph
        auto log = logger{};
        auto prof = profiler::profiler_session{false};
        auto config = job_system_config{
            .performance_worker_count = 4,
            .efficiency_worker_count = 0,
        };
        auto sys = job_system{log, prof, config};

        auto graph = task_graph{};
        auto first_done = atomic<bool>{false};
        auto second_done = atomic<bool>{false};

        auto& first = graph.emplace("First", [&first_done] {
            first_done.store(true, memory_order::release);
        });
        auto& second = graph.emplace("Second", [&second_done] {
            second_done.store(true, memory_order::release);
        });
        first.precede(second);

        // 2. Act: Move graph into execute and unpack via structured bindings
        auto run = [&sys, g = tempest::move(graph)]() mutable -> task<task_graph_result, void> {
            auto [returned_graph, status] = co_await sys.execute(tempest::move(g));
            co_return task_graph_result{
                .graph = tempest::move(returned_graph),
                .status = status,
            };
        };

        auto t = run();
        t.resume();
        sys.wait_idle();

        // 3. Assert: Execution succeeded and graph was safely returned
        ASSERT_TRUE(t.is_ready());
        auto res = tempest::move(t.value());
        EXPECT_TRUE(res.has_value());
        EXPECT_TRUE(first_done.load(memory_order::acquire));
        EXPECT_TRUE(second_done.load(memory_order::acquire));
        EXPECT_EQ(res.graph.size(), 2u);
    }

    /// @brief Verifies that when a node fails in a moved graph, execution safely returns
    ///        the graph inside task_graph_result, preserves the error code, and allows inspecting failing nodes.
    TEST(task_graph_test, by_value_execution_error_recovery)
    {
        // 1. Setup: Graph with a failing node and dependent child
        auto log = logger{};
        auto prof = profiler::profiler_session{false};
        auto config = job_system_config{
            .performance_worker_count = 4,
            .efficiency_worker_count = 0,
        };
        auto sys = job_system{log, prof, config};

        auto graph = task_graph{};
        auto root_done = atomic<bool>{false};
        auto child_executed = atomic<bool>{false};

        auto& root = graph.emplace("Root", [&root_done] {
            root_done.store(true, memory_order::release);
        });
        auto& failing = graph.emplace("Failing", []() -> expected<void, job_error> {
            return unexpected{job_error::task_failed};
        });
        auto& child = graph.emplace("Child", [&child_executed] {
            child_executed.store(true, memory_order::release);
        });

        root.precede(failing);
        failing.precede(child);

        // 2. Act: Execute by value
        auto run = [&sys, g = tempest::move(graph)]() mutable -> task<task_graph_result, void> {
            co_return co_await sys.execute(tempest::move(g));
        };

        auto t = run();
        t.resume();
        sys.wait_idle();

        // 3. Assert: Graph is recovered, failure reported, and downstream pruned
        ASSERT_TRUE(t.is_ready());
        auto res = tempest::move(t.value());
        EXPECT_FALSE(res.has_value());
        EXPECT_EQ(res.error(), job_error::task_failed);
        EXPECT_TRUE(root_done.load(memory_order::acquire));
        EXPECT_FALSE(child_executed.load(memory_order::acquire));

        // Invariant: Failing node's result is inspectable on the restored graph
        ASSERT_EQ(res.graph.size(), 3u);
        auto found_failing = false;
        for (const auto& node : res.graph.nodes())
        {
            if (node->name() == "Failing")
            {
                found_failing = true;
                EXPECT_FALSE(node->result().has_value());
                EXPECT_EQ(node->result().error(), job_error::task_failed);
            }
        }
        EXPECT_TRUE(found_failing);
    }

    /// @brief Verifies that a task_graph can be repeatedly moved into execute across frames,
    ///        restoring ownership each frame with zero memory reallocations.
    TEST(task_graph_test, by_value_reusable_frame_execution)
    {
        // 1. Setup: 4-worker job system and reusable DAG
        auto log = logger{};
        auto prof = profiler::profiler_session{false};
        auto config = job_system_config{
            .performance_worker_count = 4,
            .efficiency_worker_count = 0,
        };
        auto sys = job_system{log, prof, config};

        auto graph = task_graph{};
        auto iteration_count = atomic<int>{0};

        auto& n1 = graph.emplace("Node1", [&iteration_count] {
            iteration_count.fetch_add(1, memory_order::relaxed);
        });
        auto& n2 = graph.emplace("Node2", [&iteration_count] {
            iteration_count.fetch_add(1, memory_order::relaxed);
        });
        n1.precede(n2);

        // 2. Act: Execute 5 frames consecutively transferring ownership in and out
        for (auto frame = 0; frame < 5; ++frame)
        {
            auto run_frame = [&sys, g = tempest::move(graph)]() mutable -> task<task_graph_result, void> {
                co_return co_await sys.execute(tempest::move(g));
            };

            auto t = run_frame();
            t.resume();
            sys.wait_idle();

            ASSERT_TRUE(t.is_ready());
            auto res = tempest::move(t.value());
            ASSERT_TRUE(res.has_value());

            // Regain graph ownership for next frame
            graph = tempest::move(res.graph);
        }

        // 3. Assert: 2 nodes executed 5 times = 10 invocations, and graph ownership intact
        EXPECT_EQ(iteration_count.load(memory_order::relaxed), 10);
        EXPECT_EQ(graph.size(), 2u);
    }
} // namespace tempest::job::tests
