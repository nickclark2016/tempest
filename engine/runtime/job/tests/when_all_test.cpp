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
        // 1. Setup: 4-worker job system
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

        // 2. Act: Execute when_all with variadic tasks
        auto run = [&]() -> task<void> {
            auto results = co_await when_all(t1(), t2(), t3());

            // 3. Assert: Verify each tuple element
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
        // 1. Setup: 4-worker job system
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

        // 2. Act: Execute dynamic when_all
        auto run = [&]() -> task<void> {
            auto results = co_await when_all(tempest::move(tasks));

            // 3. Assert: All 8 results match
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
        // 1. Setup: 4-worker job system
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

        // 2. Act: Execute when_all containing both success and failure tasks
        auto run = [&]() -> task<void> {
            auto results = co_await when_all(success_task(), fail_task(), void_fail_task());

            // 3. Assert:
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
        // 1. Setup: 4-worker job system
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

        // 2. Act: Structured bindings with co_await when_all(...)
        auto run = [&]() -> task<void> {
            auto [r1, r2, r3] = co_await when_all(t1(), t2(), t3());

            // 3. Assert: Unpacked expected results
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
        // 1. Setup
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

        // 2. Act
        auto run = [&]() -> task<void> {
            auto [r1, r2] = co_await when_all(t1(), t2());

            // 3. Assert: monadic functional chaining across structured binding results
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
} // namespace tempest::job::tests
