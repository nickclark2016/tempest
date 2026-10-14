#include <gtest/gtest.h>

#include <tempest/job/job_system.hpp>
#include <tempest/job/when_all.hpp>
#include <tempest/logger.hpp>
#include <tempest/profiler/session.hpp>
#include <tempest/vector.hpp>

namespace tempest::job::tests
{
    // =========================================================================
    // SECTION: Variadic when_all Execution
    // =========================================================================

    /// @brief Verifies that variadic when_all executes tasks in parallel and returns
    ///        a tuple of expected<T, E> results matching input types.
    TEST(when_all_test, variadic_when_all_success)
    {
        // Setup: 4-worker job system
        auto log = logger{};
        auto prof = profiler::profiler_session{false};
        auto config = job_system_config{
            .performance_worker_count = 4,
            .efficiency_worker_count = 0,
        };
        auto sys = job_system{log, prof, config};

        auto t1 = []() -> task<int> {
            co_return 42;
        };
        auto t2 = []() -> task<void> {
            co_return;
        };
        auto t3 = []() -> task<int> {
            co_return 100;
        };

        // Act: Execute when_all with variadic tasks dispatched to job_system
        auto run = [&]() -> task<void> {
            auto results = co_await when_all(sys, t1(), t2(), t3());

            // Assert: Verify each tuple element
            EXPECT_TRUE(get<0>(results).has_value());
            EXPECT_EQ(get<0>(results).value(), 42);

            EXPECT_TRUE(get<1>(results).has_value());

            EXPECT_TRUE(get<2>(results).has_value());
            EXPECT_EQ(get<2>(results).value(), 100);
        };

        auto main_task = run();
        main_task.resume();
        sys.wait_idle();

        ASSERT_TRUE(main_task.is_ready());
    }

    // =========================================================================
    // SECTION: Dynamic Vector when_all Execution
    // =========================================================================

    /// @brief Verifies that when_all accepts a dynamic vector of tasks and returns
    ///        a vector of expected<T, E> results.
    TEST(when_all_test, dynamic_vector_when_all)
    {
        // Setup: 4-worker job system
        auto log = logger{};
        auto prof = profiler::profiler_session{false};
        auto config = job_system_config{
            .performance_worker_count = 4,
            .efficiency_worker_count = 0,
        };
        auto sys = job_system{log, prof, config};

        constexpr auto count = 8;
        auto task_gen = [](int idx) -> task<int> {
            co_return idx * 10;
        };

        auto tasks = vector<task<int>>{};
        tasks.reserve(count);
        for (auto i = 0; i < count; ++i)
        {
            tasks.push_back(task_gen(i));
        }

        // Act: Execute dynamic when_all
        auto run = [&]() -> task<void> {
            auto results = co_await when_all(sys, tempest::move(tasks));

            // Assert: All 8 results match
            EXPECT_EQ(results.size(), static_cast<size_t>(count));
            for (auto i = 0u; i < static_cast<size_t>(count); ++i)
            {
                EXPECT_TRUE(results[i].has_value());
                EXPECT_EQ(results[i].value(), static_cast<int>(i * 10));
            }
        };

        auto main_task = run();
        main_task.resume();
        sys.wait_idle();

        ASSERT_TRUE(main_task.is_ready());
    }

    // =========================================================================
    // SECTION: Structured Error Collection (No Short-Circuit)
    // =========================================================================

    /// @brief Verifies that when_all collects partial failures without prematurely
    ///        aborting, preserving expected<T, E> error codes alongside successful values.
    TEST(when_all_test, structured_error_collection)
    {
        // Setup: 4-worker job system
        auto log = logger{};
        auto prof = profiler::profiler_session{false};
        auto config = job_system_config{
            .performance_worker_count = 4,
            .efficiency_worker_count = 0,
        };
        auto sys = job_system{log, prof, config};

        auto success_task = []() -> task<int> {
            co_return 999;
        };

        auto fail_task = []() -> task<int> {
            co_await unexpected{job_error::task_failed};
            co_return 0;
        };

        auto void_fail_task = []() -> task<void> {
            co_await unexpected{job_error::channel_closed};
            co_return;
        };

        // Act: Execute when_all containing both success and failure tasks via sys.when_all
        auto run = [&]() -> task<void> {
            auto results = co_await sys.when_all(success_task(), fail_task(), void_fail_task());

            // Assert:
            // Element 0: Succeeded with value 999
            EXPECT_TRUE(get<0>(results).has_value());
            EXPECT_EQ(get<0>(results).value(), 999);

            // Element 1: Failed with task_failed
            EXPECT_FALSE(get<1>(results).has_value());
            EXPECT_EQ(get<1>(results).error(), job_error::task_failed);

            // Element 2: Failed with channel_closed
            EXPECT_FALSE(get<2>(results).has_value());
            EXPECT_EQ(get<2>(results).error(), job_error::channel_closed);
        };

        auto main_task = run();
        main_task.resume();
        sys.wait_idle();

        ASSERT_TRUE(main_task.is_ready());
    }

    // =========================================================================
    // SECTION: Structured Binding Unpacking
    // =========================================================================

    /// @brief Verifies that structured bindings unpack variadic when_all results cleanly.
    TEST(when_all_test, structured_binding_unpacking)
    {
        // Setup: 4-worker job system
        auto log = logger{};
        auto prof = profiler::profiler_session{false};
        auto config = job_system_config{
            .performance_worker_count = 4,
            .efficiency_worker_count = 0,
        };
        auto sys = job_system{log, prof, config};

        auto t1 = []() -> task<int> {
            co_return 10;
        };
        auto t2 = []() -> task<string_view> {
            co_return string_view{"hello"};
        };
        auto t3 = []() -> task<void> {
            co_return;
        };

        // Act: Structured bindings with co_await sys.when_all(...)
        auto run = [&]() -> task<void> {
            auto [r1, r2, r3] = co_await sys.when_all(t1(), t2(), t3());

            // Assert: Unpacked expected results
            EXPECT_TRUE(r1.has_value());
            EXPECT_EQ(r1.value(), 10);

            EXPECT_TRUE(r2.has_value());
            EXPECT_EQ(r2.value(), "hello");

            EXPECT_TRUE(r3.has_value());
        };

        auto main_task = run();
        main_task.resume();
        sys.wait_idle();

        ASSERT_TRUE(main_task.is_ready());
    }

    /// @brief Verifies that results unpacked via structured bindings from when_all
    ///        can be functionally chained using monadic expected operators.
    TEST(when_all_test, structured_binding_monadic_chaining)
    {
        // Setup
        auto log = logger{};
        auto prof = profiler::profiler_session{false};
        auto cfg = job_system_config{
            .performance_worker_count = 2,
            .efficiency_worker_count = 0,
            .enable_core_pinning = false,
        };
        auto sys = job_system{log, prof, cfg};

        auto t1 = []() -> task<int> { co_return 25; };
        auto t2 = []() -> task<int> { co_return 17; };

        // Act
        auto run = [&]() -> task<void> {
            auto [r1, r2] = co_await when_all(t1(), t2());

            // Assert: monadic functional chaining across structured binding results
            auto combined = r1.and_then([&](int a) {
                return r2.transform([&](int b) { return a + b; });
            });

            EXPECT_TRUE(combined.has_value());
            EXPECT_EQ(combined.value(), 42);
        };

        auto main_task = run();
        main_task.resume();
        sys.wait_idle();

        ASSERT_TRUE(main_task.is_ready());
    }

    /// @brief Verifies that dynamic vector when_all executes standalone without any job_system.
    TEST(when_all_test, standalone_vector_when_all)
    {
        // Setup
        auto t1 = []() -> task<int> { co_return 100; };
        auto t2 = []() -> task<int> { co_return 200; };
        auto tasks = vector<task<int>>{};
        tasks.push_back(t1());
        tasks.push_back(t2());

        // Act
        auto run = [&]() -> task<void> {
            auto res = co_await when_all(tempest::move(tasks));

            // Assert
            EXPECT_EQ(res.size(), 2u);
            EXPECT_TRUE(res[0].has_value());
            EXPECT_EQ(res[0].value(), 100);
            EXPECT_TRUE(res[1].has_value());
            EXPECT_EQ(res[1].value(), 200);
        };

        auto main_task = run();
        main_task.resume();
        EXPECT_TRUE(main_task.is_ready());
    }

    /// @brief Verifies that when_all with job_system routes leaf runner and awaiter
    ///        allocations through the job allocator without triggering reference parameter diagnostics.
    TEST(when_all_test, when_all_routes_allocator_to_dispatch_allocator)
    {
        // Setup
        auto log = logger{};
        auto prof = profiler::profiler_session{false};
        auto config = job_system_config{
            .performance_worker_count = 2,
            .efficiency_worker_count = 0,
        };
        auto sys = job_system{log, prof, config};
        auto& alloc = sys.get_dispatch_allocator();

        auto t1 = []() -> task<int> { co_return 10; };
        auto t2 = []() -> task<int> { co_return 20; };

        // Act
        const auto telem_before = alloc.get_telemetry();
        auto run = [&]() -> task<void> {
            auto [r1, r2] = co_await when_all(sys, t1(), t2());
            EXPECT_TRUE(r1.has_value());
            EXPECT_EQ(r1.value(), 10);
            EXPECT_TRUE(r2.has_value());
            EXPECT_EQ(r2.value(), 20);
        };

        auto main_task = run();
        main_task.resume();
        sys.wait_idle();

        // Assert: Slabs were utilized by when_all runner and awaiter frames
        const auto telem_after = alloc.get_telemetry();
        auto slab_allocated = false;
        for (size_t i = 0; i < slab_class_count; ++i)
        {
            if (telem_after.allocations_per_class[i] > telem_before.allocations_per_class[i])
            {
                slab_allocated = true;
                break;
            }
        }
        EXPECT_TRUE(slab_allocated);
        EXPECT_TRUE(main_task.is_ready());
    }

    // =========================================================================
    // SECTION: Profiler Provenance & Leaf Naming
    // =========================================================================

    /// @brief Verifies that when_all leaves adopt the child tasks' names instead of
    ///        "detached_task", and that their coroutine provenance correctly links
    ///        to the parent task.
    TEST(when_all_test, when_all_leaf_naming_and_provenance)
    {
        // Setup: 4-worker job system with active profiler
        auto log = logger{};
        auto prof = profiler::profiler_session{true};
        auto config = job_system_config{
            .performance_worker_count = 4,
            .efficiency_worker_count = 0,
        };
        auto sys = job_system{log, prof, config};

        auto make_leaf = [](int val) -> task<int> {
            co_return val * 3;
        };

        auto t1 = make_leaf(10).with_name("leaf_alpha");
        auto t2 = make_leaf(20).with_name("leaf_beta");

        // Act: Execute when_all inside a parent task
        auto parent = [&]() -> task<void> {
            auto [r1, r2] = co_await when_all(sys, tempest::move(t1), tempest::move(t2));
            EXPECT_TRUE(r1.has_value());
            EXPECT_EQ(r1.value(), 30);
            EXPECT_TRUE(r2.has_value());
            EXPECT_EQ(r2.value(), 60);
        };

        auto parent_job = parent().with_name("parent_when_all_task").with_profiler(prof);
        const auto parent_cid = parent_job.coroutine_id();
        EXPECT_GT(parent_cid, 0U);

        sys.schedule(parent_job);
        sys.wait_idle();

        // Assert: Verify completion and profiler chunk zones
        EXPECT_TRUE(parent_job.is_ready());

        auto chunks = prof.drain_completed_chunks();
        ASSERT_FALSE(chunks.empty());

        auto found_alpha = false;
        auto found_beta = false;

        for (const auto& chunk : chunks)
        {
            for (const auto& zone : chunk->zones())
            {
                // Invariant: No zone should ever be titled "detached_task"
                EXPECT_NE(zone.name, "detached_task");

                if (zone.name == "leaf_alpha")
                {
                    found_alpha = true;
                    EXPECT_GT(zone.spawned_by_thread_id, 0U);
                    EXPECT_EQ(zone.spawned_by_coroutine_id, parent_cid);
                }
                else if (zone.name == "leaf_beta")
                {
                    found_beta = true;
                    EXPECT_GT(zone.spawned_by_thread_id, 0U);
                    EXPECT_EQ(zone.spawned_by_coroutine_id, parent_cid);
                }
            }
        }

        EXPECT_TRUE(found_alpha);
        EXPECT_TRUE(found_beta);
    }
} // namespace tempest::job::tests
