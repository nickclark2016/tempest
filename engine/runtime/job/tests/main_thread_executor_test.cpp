#include <gtest/gtest.h>

#include <tempest/atomic.hpp>
#include <tempest/int.hpp>
#include <tempest/job/main_thread_executor.hpp>
#include <tempest/job/task.hpp>
#include <tempest/thread.hpp>
#include <tempest/vector.hpp>

namespace tempest::job::tests
{
    namespace
    {
        auto async_worker_task(main_thread_executor& executor,
                               tempest::thread::id* background_id_out,
                               tempest::thread::id* resumed_id_out,
                               atomic<bool>* completed_out) -> task<void, void>
        {
            *background_id_out = tempest::this_thread::get_id();
            co_await resume_on_main_thread{executor};
            *resumed_id_out = tempest::this_thread::get_id();
            completed_out->store(true, memory_order::release);
            co_return;
        }
    } // namespace

    // =========================================================================
    // SECTION: Sequential Callback Execution
    // =========================================================================

    /// @brief Verifies that callbacks posted to main_thread_executor execute in exact FIFO order
    ///        when dispatch_pending() is invoked on the main thread.
    TEST(job_main_thread_executor_test, post_callback_executes_in_order)
    {
        // 1. Setup: Construct an empty main_thread_executor
        auto executor = main_thread_executor{};
        EXPECT_TRUE(executor.empty());
        EXPECT_EQ(executor.capacity(), 2048u);

        auto execution_order = vector<int>{};

        // 2. Act: Post sequential callbacks recording ordered values
        executor.post([&execution_order]() { execution_order.push_back(10); });
        executor.post([&execution_order]() { execution_order.push_back(20); });
        executor.post([&execution_order]() { execution_order.push_back(30); });
        executor.post([&execution_order]() { execution_order.push_back(40); });
        executor.post([&execution_order]() { execution_order.push_back(50); });

        EXPECT_FALSE(executor.empty());

        // Drain pending tasks on main thread
        const auto dispatched = executor.dispatch_pending();

        // 3. Assert: All callbacks executed in exact FIFO order
        EXPECT_EQ(dispatched, 5u);
        EXPECT_TRUE(executor.empty());
        ASSERT_EQ(execution_order.size(), 5u);
        EXPECT_EQ(execution_order[0], 10);
        EXPECT_EQ(execution_order[1], 20);
        EXPECT_EQ(execution_order[2], 30);
        EXPECT_EQ(execution_order[3], 40);
        EXPECT_EQ(execution_order[4], 50);
    }

    /// @brief Verifies that dispatch_pending() returns 0 when the executor is empty.
    TEST(job_main_thread_executor_test, empty_executor_dispatch_returns_zero)
    {
        // 1. Setup: Empty executor
        auto executor = main_thread_executor{};
        EXPECT_TRUE(executor.empty());

        // 2. Act: Dispatch on empty executor
        const auto dispatched = executor.dispatch_pending();

        // 3. Assert: Zero tasks were dispatched
        EXPECT_EQ(dispatched, 0u);
        EXPECT_TRUE(executor.empty());
    }

    /// @brief Verifies that a callback executing within dispatch_pending() can post a new callback,
    ///        and that the subsequent callback executes properly.
    TEST(job_main_thread_executor_test, nested_callback_posts_and_dispatches)
    {
        // 1. Setup: Executor and state tracking
        auto executor = main_thread_executor{};
        auto nested_executed = false;

        // 2. Act: Post outer task which posts inner task
        executor.post([&executor, &nested_executed]() {
            executor.post([&nested_executed]() {
                nested_executed = true;
            });
        });

        const auto first_dispatched = executor.dispatch_pending();

        // 3. Assert: Outer task ran, inner task ran (or drained)
        EXPECT_GT(first_dispatched, 0u);
        if (!nested_executed)
        {
            const auto second_dispatched = executor.dispatch_pending();
            EXPECT_EQ(second_dispatched, 1u);
        }
        EXPECT_TRUE(nested_executed);
        EXPECT_TRUE(executor.empty());
    }

    // =========================================================================
    // SECTION: Coroutine Resumption on Main Thread
    // =========================================================================

    /// @brief Verifies that a coroutine initiated on a background worker thread suspends,
    ///        schedules resumption via resume_on_main_thread, and resumes strictly on the
    ///        main thread with matching thread identity.
    TEST(job_main_thread_executor_test, coroutine_resumes_on_main_thread)
    {
        // 1. Setup: Executor and thread identity trackers
        auto executor = main_thread_executor{};
        const auto main_thread_id = tempest::this_thread::get_id();
        auto background_thread_id = tempest::thread::id{};
        auto resumed_thread_id = tempest::thread::id{};
        auto completed = atomic<bool>{false};

        auto worker_task = async_worker_task(executor, &background_thread_id, &resumed_thread_id, &completed);

        // 2. Act: Resume the coroutine on a background thread until it awaits resume_on_main_thread
        auto worker = tempest::thread{[&worker_task]() {
            worker_task.resume();
        }};
        worker.join();

        // The background worker suspended at resume_on_main_thread and queued to executor
        EXPECT_FALSE(completed.load(memory_order::acquire));
        EXPECT_FALSE(executor.empty());

        // Dispatch on main thread
        const auto dispatched = executor.dispatch_pending();

        // 3. Assert: Resumed on the main thread and completed
        EXPECT_EQ(dispatched, 1u);
        EXPECT_TRUE(completed.load(memory_order::acquire));
        EXPECT_NE(background_thread_id, main_thread_id);
        EXPECT_EQ(resumed_thread_id, main_thread_id);
        EXPECT_TRUE(executor.empty());
    }

    // =========================================================================
    // SECTION: Multi-Threaded Concurrent High Contention
    // =========================================================================

    /// @brief Verifies that 16 concurrent background threads can continuously post a total of 50,000 tasks
    ///        to main_thread_executor under heavy contention without dropping tasks or corrupting state,
    ///        while the main thread concurrently pumps dispatch_pending().
    TEST(job_main_thread_executor_test, high_contention_stress_test)
    {
        // 1. Setup: Initialize executor and synchronization counters
        constexpr auto thread_count = size_t{16};
        constexpr auto tasks_per_thread = size_t{3125};
        constexpr auto total_tasks = thread_count * tasks_per_thread; // Exactly 50,000 tasks

        auto executor = main_thread_executor{};
        auto executed_count = atomic<size_t>{0};
        auto threads_finished = atomic<size_t>{0};

        // 2. Act: Spawn 16 concurrent background worker threads posting tasks
        auto workers = vector<tempest::thread>{};
        workers.reserve(thread_count);

        for (auto thread_idx = size_t{0}; thread_idx < thread_count; ++thread_idx)
        {
            workers.push_back(tempest::thread{[&executor, &executed_count, &threads_finished]() {
                for (auto task_idx = size_t{0}; task_idx < tasks_per_thread; ++task_idx)
                {
                    executor.post([&executed_count]() {
                        executed_count.fetch_add(1, memory_order::relaxed);
                    });
                }
                threads_finished.fetch_add(1, memory_order::release);
            }});
        }

        // Main thread continuously pumps dispatch_pending() until all background threads finish and queue is empty
        auto total_dispatched = size_t{0};
        while (threads_finished.load(memory_order::acquire) < thread_count || !executor.empty())
        {
            total_dispatched += executor.dispatch_pending();
            tempest::this_thread::yield();
        }

        for (auto& worker : workers)
        {
            worker.join();
        }

        // Final pump to drain any trailing items
        total_dispatched += executor.dispatch_pending();

        // 3. Assert: Exactly 50,000 tasks were dispatched and executed with zero dropped items
        EXPECT_EQ(total_dispatched, total_tasks);
        EXPECT_EQ(executed_count.load(memory_order::acquire), total_tasks);
        EXPECT_TRUE(executor.empty());
    }
} // namespace tempest::job::tests
