#include <gtest/gtest.h>

#include <tempest/atomic.hpp>
#include <tempest/job/async_event.hpp>
#include <tempest/job/job_system.hpp>
#include <tempest/job/task.hpp>
#include <tempest/logger.hpp>
#include <tempest/profiler/session.hpp>
#include <tempest/thread.hpp>
#include <tempest/vector.hpp>

namespace tempest::job::tests
{
    namespace
    {
        auto waiter_task(async_event& evt, bool* flag) -> task<void>
        {
            co_await evt.wait();
            *flag = true;
            co_return;
        }

        auto record_wake_order_task(async_event& evt, int id, vector<int>* order) -> task<void>
        {
            co_await evt.wait();
            order->push_back(id);
            co_return;
        }

        auto atomic_counter_waiter(async_event& evt, atomic<int>* counter) -> task<void>
        {
            co_await evt.wait();
            counter->fetch_add(1, memory_order::relaxed);
            co_return;
        }
    } // namespace

    // =========================================================================
    // SECTION: Manual-Reset Event Tests
    // =========================================================================

    /// @brief Verifies that setting a manual-reset event awakens all currently waiting
    ///        coroutines and allows subsequent waiters to pass without suspending until reset.
    TEST(async_event_test, manual_reset_broadcast)
    {
        // 1. Setup
        auto evt = async_event{false, event_reset_mode::manual};
        auto w1_ran = false;
        auto w2_ran = false;
        auto w3_ran = false;

        auto t1 = waiter_task(evt, &w1_ran);
        auto t2 = waiter_task(evt, &w2_ran);

        t1.resume();
        t2.resume();

        EXPECT_FALSE(w1_ran);
        EXPECT_FALSE(w2_ran);

        // 2. Act: Set event
        evt.set();

        // 3. Assert: Both waiters awakened
        EXPECT_TRUE(w1_ran);
        EXPECT_TRUE(w2_ran);
        EXPECT_TRUE(evt.is_set());

        // New waiter arrives while event is still set
        auto t3 = waiter_task(evt, &w3_ran);
        t3.resume();
        EXPECT_TRUE(w3_ran);

        // Reset clears signaled state
        evt.reset();
        EXPECT_FALSE(evt.is_set());
    }

    /// @brief Verifies that an event initialized in the signaled state passes waiters immediately.
    TEST(async_event_test, manual_reset_pre_signaled)
    {
        // 1. Setup: Manual event initialized to true
        auto evt = async_event{true, event_reset_mode::manual};
        EXPECT_TRUE(evt.is_set());

        auto ran = false;

        // 2. Act: Wait on already signaled event
        auto t = waiter_task(evt, &ran);
        t.resume();

        // 3. Assert: Waiter completed immediately without suspending
        EXPECT_TRUE(ran);
        EXPECT_TRUE(evt.is_set());
    }

    /// @brief Verifies concurrent multi-threaded broadcast on a manual-reset event.
    TEST(async_event_test, manual_reset_concurrent_broadcast_stress)
    {
        // 1. Setup: 16 waiting threads, each running a waiter coroutine
        constexpr auto thread_count = 16;
        auto evt = async_event{false, event_reset_mode::manual};
        auto completed_count = atomic<int>{0};
        auto ready_count = atomic<int>{0};

        auto tasks = vector<task<void>>{};
        tasks.reserve(thread_count);
        for (auto index = 0; index < thread_count; ++index)
        {
            tasks.push_back(atomic_counter_waiter(evt, &completed_count));
        }

        auto threads = vector<tempest::thread>{};
        threads.reserve(thread_count);

        for (auto index = 0; index < thread_count; ++index)
        {
            threads.push_back(tempest::thread([&tasks, &ready_count, index]() {
                ready_count.fetch_add(1, memory_order::release);
                tasks[static_cast<size_t>(index)].resume();
            }));
        }

        // Wait until all threads have begun execution
        while (ready_count.load(memory_order::acquire) < thread_count)
        {
            // Spin until all threads are ready
        }

        // 2. Act: Signal the event concurrently from multiple setter threads
        auto s1 = tempest::thread([&evt]() { evt.set(); });
        auto s2 = tempest::thread([&evt]() { evt.set(); });
        s1.join();
        s2.join();

        for (auto& thread : threads)
        {
            thread.join();
        }

        // 3. Assert: All waiters were awakened
        EXPECT_EQ(completed_count.load(memory_order::relaxed), thread_count);
        EXPECT_TRUE(evt.is_set());

        // Reset clears state
        evt.reset();
        EXPECT_FALSE(evt.is_set());
    }

    // =========================================================================
    // SECTION: Auto-Reset Event Tests
    // =========================================================================

    /// @brief Verifies that setting an auto-reset event awakens exactly one waiter,
    ///        leaving subsequent waiters suspended until the event is set again.
    TEST(async_event_test, auto_reset_single_wake)
    {
        // 1. Setup
        auto evt = async_event{false, event_reset_mode::auto_reset};
        auto w1_ran = false;
        auto w2_ran = false;

        auto t1 = waiter_task(evt, &w1_ran);
        auto t2 = waiter_task(evt, &w2_ran);

        t1.resume();
        t2.resume();

        EXPECT_FALSE(w1_ran);
        EXPECT_FALSE(w2_ran);

        // 2. Act: First set
        evt.set();

        // 3. Assert: Exactly one waiter should have run
        EXPECT_TRUE(w1_ran ^ w2_ran);
        EXPECT_FALSE(evt.is_set()); // Automatically reset

        // Second set awakens the remaining waiter
        evt.set();
        EXPECT_TRUE(w1_ran && w2_ran);
        EXPECT_FALSE(evt.is_set());
    }

    /// @brief Verifies that pre-signaled auto-reset event allows the first waiter to pass
    ///        and automatically resets, suspending the second waiter until explicitly set.
    TEST(async_event_test, auto_reset_pre_signaled)
    {
        // 1. Setup: Auto-reset event initialized to true
        auto evt = async_event{true, event_reset_mode::auto_reset};
        EXPECT_TRUE(evt.is_set());

        auto w1_ran = false;
        auto w2_ran = false;

        // 2. Act: First waiter passes immediately
        auto t1 = waiter_task(evt, &w1_ran);
        t1.resume();

        EXPECT_TRUE(w1_ran);
        EXPECT_FALSE(evt.is_set()); // Signal was consumed

        // Second waiter suspends
        auto t2 = waiter_task(evt, &w2_ran);
        t2.resume();
        EXPECT_FALSE(w2_ran);

        // 3. Act & Assert: Setting event awakens the second waiter
        evt.set();
        EXPECT_TRUE(w2_ran);
        EXPECT_FALSE(evt.is_set());
    }

    /// @brief Verifies that multiple waiters enqueued on an auto-reset event are
    ///        awakened in strict First-In, First-Out (FIFO) order through ingress/egress transfer.
    TEST(async_event_test, auto_reset_strict_fifo_wake)
    {
        // 1. Setup
        auto evt = async_event{false, event_reset_mode::auto_reset};
        auto wake_order = vector<int>{};

        auto t1 = record_wake_order_task(evt, 1, &wake_order);
        auto t2 = record_wake_order_task(evt, 2, &wake_order);
        auto t3 = record_wake_order_task(evt, 3, &wake_order);

        t1.resume();
        t2.resume();
        t3.resume();

        EXPECT_TRUE(wake_order.empty());

        // 2. Act: Successively trigger set() 3 times
        evt.set();
        ASSERT_EQ(wake_order.size(), 1u);
        EXPECT_EQ(wake_order[0], 1);

        evt.set();
        ASSERT_EQ(wake_order.size(), 2u);
        EXPECT_EQ(wake_order[1], 2);

        evt.set();
        ASSERT_EQ(wake_order.size(), 3u);
        EXPECT_EQ(wake_order[2], 3);

        // 3. Assert: All coroutines completed in FIFO order
        EXPECT_TRUE(t1.is_ready());
        EXPECT_TRUE(t2.is_ready());
        EXPECT_TRUE(t3.is_ready());
    }

    /// @brief Verifies flat combining under concurrent set() calls from multiple threads.
    TEST(async_event_test, auto_reset_concurrent_flat_combining_stress)
    {
        // 1. Setup: 8 waiters and 8 concurrent setter threads
        constexpr auto count = 8;
        auto evt = async_event{false, event_reset_mode::auto_reset};
        auto completed_counter = atomic<int>{0};

        auto tasks = vector<task<void>>{};
        tasks.reserve(count);
        for (auto index = 0; index < count; ++index)
        {
            tasks.push_back(atomic_counter_waiter(evt, &completed_counter));
            tasks.back().resume();
        }

        EXPECT_EQ(completed_counter.load(memory_order::relaxed), 0);

        // 2. Act: Spawn 8 concurrent setter threads
        auto setters = vector<tempest::thread>{};
        setters.reserve(count);
        for (auto index = 0; index < count; ++index)
        {
            setters.push_back(tempest::thread([&evt]() {
                evt.set();
            }));
        }

        for (auto& setter : setters)
        {
            setter.join();
        }

        // 3. Assert: Exactly all 8 waiters were awakened
        EXPECT_EQ(completed_counter.load(memory_order::relaxed), count);
    }

    /// @brief Stresses producer-consumer coordination using auto-reset events and job_system workers.
    TEST(async_event_test, auto_reset_job_system_worker_stress)
    {
        // 1. Setup: Job system with 4 performance workers
        auto log = logger{};
        auto prof = profiler::profiler_session{false};
        auto config = job_system_config{
            .performance_worker_count = 4,
            .efficiency_worker_count = 0,
        };
        auto sys = job_system{log, prof, config};

        auto evt_work = async_event{sys, false, event_reset_mode::auto_reset};
        auto evt_done = async_event{false, event_reset_mode::auto_reset};
        constexpr auto total_iterations = 200;
        auto consumed_counter = atomic<int>{0};

        // Worker coroutine consuming signals in a loop
        auto consumer_coro = [&evt_work, &evt_done, &consumed_counter]() -> task<void> {
            for (auto index = 0; index < total_iterations; ++index)
            {
                co_await evt_work.wait();
                consumed_counter.fetch_add(1, memory_order::relaxed);
                evt_done.set();
            }
            co_return;
        };

        // 2. Act: Launch consumer task on worker pool
        auto consumer_task = consumer_coro();
        sys.schedule(consumer_task.handle());

        // Producer waiting on consumer via ping-pong
        auto producer_coro = [&evt_work, &evt_done]() -> task<void> {
            for (auto index = 0; index < total_iterations; ++index)
            {
                evt_work.set();
                co_await evt_done.wait();
            }
            co_return;
        };

        auto producer_task = producer_coro();
        producer_task.resume();

        sys.wait_idle();

        // 3. Assert: All signals were consumed
        EXPECT_EQ(consumed_counter.load(memory_order::relaxed), total_iterations);
    }

    /// @brief Verifies that concurrent setters and waiters on an auto-reset event never suffer from lost wakeups.
    TEST(async_event_test, auto_reset_lost_wakeup_stress)
    {
        // 1. Setup: 4 waiter threads, 4 setter threads
        constexpr auto iterations = 500;
        auto evt = async_event{false, event_reset_mode::auto_reset};
        auto completed_count = atomic<int>{0};
        auto stop_flag = atomic<bool>{false};

        auto waiter_coro = [&evt, &completed_count, &stop_flag]() -> task<void> {
            while (!stop_flag.load(memory_order::acquire))
            {
                co_await evt.wait();
                completed_count.fetch_add(1, memory_order::relaxed);
            }
            co_return;
        };

        auto threads = vector<tempest::thread>{};
        constexpr auto waiter_thread_count = 4;
        constexpr auto setter_thread_count = 4;

        auto tasks = vector<task<void>>{};
        tasks.reserve(waiter_thread_count);

        for (auto index = 0; index < waiter_thread_count; ++index)
        {
            tasks.push_back(waiter_coro());
            threads.push_back(tempest::thread([&tasks, index]() {
                tasks[static_cast<size_t>(index)].resume();
            }));
        }

        // 2. Act: Setters pulse the event repeatedly
        for (auto index = 0; index < setter_thread_count; ++index)
        {
            threads.push_back(tempest::thread([&evt, iterations]() {
                for (auto it = 0; it < iterations; ++it)
                {
                    evt.set();
                }
            }));
        }

        // Wait until at least a large number of signals have been consumed
        while (completed_count.load(memory_order::acquire) < (setter_thread_count * iterations))
        {
            // Spin until all sets are consumed
        }

        stop_flag.store(true, memory_order::release);
        // Wake up remaining waiting threads to exit
        for (auto index = 0; index < waiter_thread_count; ++index)
        {
            evt.set();
        }

        for (auto& thread : threads)
        {
            thread.join();
        }

        // 3. Assert: All expected signals were consumed
        EXPECT_GE(completed_count.load(memory_order::relaxed), setter_thread_count * iterations);
    }

    /// @brief Verifies that an event can be destroyed immediately by the awakened task without use-after-free.
    TEST(async_event_test, destruct_immediately_on_wake)
    {
        // 1. Setup
        for (auto iteration = 0; iteration < 100; ++iteration)
        {
            auto* evt = new async_event{false, event_reset_mode::auto_reset};
            auto done = atomic<bool>{false};

            auto destroy_on_wake = [&evt, &done]() -> task<void> {
                co_await evt->wait();
                delete evt;
                evt = nullptr;
                done.store(true, memory_order::release);
                co_return;
            };

            auto t = destroy_on_wake();
            t.resume(); // Suspends on wait()

            // 2. Act: Signal from another thread
            auto setter = tempest::thread([evt]() {
                evt->set();
            });
            setter.join();

            // 3. Assert: Event was destroyed cleanly
            EXPECT_TRUE(done.load(memory_order::acquire));
            EXPECT_EQ(evt, nullptr);
        }
    }
} // namespace tempest::job::tests
