#include <gtest/gtest.h>

#include <tempest/atomic.hpp>
#include <tempest/job/async_mutex.hpp>
#include <tempest/job/task.hpp>
#include <tempest/thread.hpp>
#include <tempest/type_traits.hpp>
#include <tempest/vector.hpp>

namespace tempest::job::tests
{
    namespace
    {
        auto lock_and_record(async_mutex& mtx, int id, vector<int>* order) -> task<void>
        {
            auto guard = co_await mtx.scoped_lock();
            order->push_back(id);
            co_return;
        }

        auto hold_mutex_task(async_mutex& mtx, bool* acquired, bool* release_signal) -> task<void>
        {
            auto guard = co_await mtx.scoped_lock();
            *acquired = true;
            while (!(*release_signal))
            {
                // Wait for external release signal
            }
            co_return;
        }
    } // namespace

    // =========================================================================
    // SECTION: Uncontended Mutex Operations
    // =========================================================================

    /// @brief Verifies that uncontended mutex try_lock and scoped_lock acquire
    ///        immediately without yielding or suspending the calling thread.
    TEST(async_mutex_test, uncontended_fast_path)
    {
        // 1. Setup
        auto mtx = async_mutex{};

        // 2. Act & Assert: try_lock
        EXPECT_TRUE(mtx.try_lock());
        EXPECT_FALSE(mtx.try_lock()); // Already held
        mtx.unlock();

        // Act & Assert: scoped_lock in coroutine
        auto test_coro = [&mtx]() -> task<bool> {
            auto guard = co_await mtx.scoped_lock();
            co_return true;
        };

        auto t = test_coro();
        t.resume();
        EXPECT_TRUE(t.is_ready());
        EXPECT_TRUE(t.value());
        EXPECT_TRUE(mtx.try_lock()); // Mutex was released upon guard destruction
        mtx.unlock();
    }

    // =========================================================================
    // SECTION: Contended Yield & FIFO Acquisition Order
    // =========================================================================

    /// @brief Verifies that a coroutine encountering a held mutex suspends, and
    ///        is awakened to acquire the lock upon unlock.
    TEST(async_mutex_test, contended_yield_and_wakeup)
    {
        // 1. Setup
        auto mtx = async_mutex{};
        EXPECT_TRUE(mtx.try_lock()); // Manually hold the lock

        bool acquired_second = false;
        auto second_task = [&mtx, &acquired_second]() -> task<void> {
            auto guard = co_await mtx.scoped_lock();
            acquired_second = true;
            co_return;
        };

        // 2. Act
        auto t2 = second_task();
        t2.resume(); // Should suspend because lock is held
        EXPECT_FALSE(acquired_second);
        EXPECT_FALSE(t2.is_ready());

        mtx.unlock(); // Wakes t2

        // 3. Assert
        EXPECT_TRUE(acquired_second);
        EXPECT_TRUE(t2.is_ready());
    }

    /// @brief Verifies that when multiple coroutines contend for the same mutex,
    ///        they acquire the lock in strict First-In, First-Out (FIFO) order.
    TEST(async_mutex_test, strict_fifo_acquisition_order)
    {
        // 1. Setup
        auto mtx = async_mutex{};
        auto acquisition_order = vector<int>{};

        EXPECT_TRUE(mtx.try_lock()); // Lock is held initially

        // 2. Act: Enqueue 3 waiters in order 1, 2, 3
        auto t1 = lock_and_record(mtx, 1, &acquisition_order);
        auto t2 = lock_and_record(mtx, 2, &acquisition_order);
        auto t3 = lock_and_record(mtx, 3, &acquisition_order);

        t1.resume(); // Suspends
        t2.resume(); // Suspends
        t3.resume(); // Suspends

        EXPECT_TRUE(acquisition_order.empty());

        // Unlock initial lock -> hands off to t1
        mtx.unlock();

        // 3. Assert: Verify FIFO execution sequence
        ASSERT_EQ(acquisition_order.size(), 3u);
        EXPECT_EQ(acquisition_order[0], 1);
        EXPECT_EQ(acquisition_order[1], 2);
        EXPECT_EQ(acquisition_order[2], 3);

        EXPECT_TRUE(t1.is_ready());
        EXPECT_TRUE(t2.is_ready());
        EXPECT_TRUE(t3.is_ready());
    }

    /// @brief Stresses tight-loop lock and immediate unlock across concurrent coroutines to ensure no double-resumes or frame UAF.
    TEST(async_mutex_test, contended_immediate_unlock_stress)
    {
        // 1. Setup
        auto mtx = async_mutex{};
        constexpr auto iterations = 500;
        constexpr auto thread_count = 4;
        auto executed_count = atomic<int>{0};

        auto worker_coro = [&mtx, &executed_count]() -> task<void> {
            for (auto index = 0; index < iterations; ++index)
            {
                auto guard = co_await mtx.scoped_lock();
                executed_count.fetch_add(1, memory_order::relaxed);
            }
            co_return;
        };

        auto threads = vector<tempest::thread>{};
        threads.reserve(thread_count);

        auto tasks = vector<task<void>>{};
        tasks.reserve(thread_count);

        // 2. Act: Spawn threads running coroutines that contend heavily
        for (auto index = 0; index < thread_count; ++index)
        {
            tasks.push_back(worker_coro());
            threads.push_back(tempest::thread([&tasks, index]() {
                tasks[static_cast<size_t>(index)].resume();
            }));
        }

        for (auto& thread : threads)
        {
            thread.join();
        }

        // 3. Assert: All iterations executed with exclusive access, zero double resumes
        EXPECT_EQ(executed_count.load(memory_order::relaxed), thread_count * iterations);
    }

    /// @brief Verifies Store-Load sequential consistency between releasing the mutex and checking for newly enqueued waiters.
    TEST(async_mutex_test, store_load_interleaving_stress)
    {
        // 1. Setup: Alternating tight unlock and lock interleaving
        auto mtx = async_mutex{};
        auto counter = 0;
        constexpr auto loop_count = 1000;

        auto worker_coro = [&mtx, &counter]() -> task<void> {
            for (auto index = 0; index < loop_count; ++index)
            {
                auto guard = co_await mtx.scoped_lock();
                ++counter;
            }
            co_return;
        };

        // 2. Act
        auto t1 = worker_coro();
        auto t2 = worker_coro();

        auto thread1 = tempest::thread([&t1]() { t1.resume(); });
        auto thread2 = tempest::thread([&t2]() { t2.resume(); });

        thread1.join();
        thread2.join();

        // 3. Assert: Counter accurately reflects all critical section executions
        EXPECT_EQ(counter, 2 * loop_count);
    }

    /// @brief Verifies that an async_mutex allocated dynamically can be unlocked and immediately destroyed
    ///        by the awakened coroutine without memory corruption or use-after-free under TSan.
    TEST(async_mutex_test, destruct_immediately_on_unlock)
    {
        // 1. Setup: 100 iterations of allocating a mutex, suspending a waiter, and unlocking it
        for (auto iteration = 0; iteration < 100; ++iteration)
        {
            auto* mtx = new async_mutex{};
            EXPECT_TRUE(mtx->try_lock()); // Manually hold the lock

            auto done_flag = atomic<bool>{false};

            auto waiter_coroutine = [&mtx, &done_flag]() -> task<void> {
                co_await mtx->lock();
                delete mtx;
                mtx = nullptr;
                done_flag.store(true, memory_order::release);
                co_return;
            };

            auto waiter_task = waiter_coroutine();
            waiter_task.resume(); // Suspends because lock is held
            EXPECT_FALSE(done_flag.load(memory_order::acquire));

            // 2. Act: Unlock from another thread
            auto unlocking_thread = tempest::thread([mtx]() {
                mtx->unlock();
            });
            unlocking_thread.join();

            // 3. Assert: Mutex was acquired, destroyed, and task completed cleanly
            EXPECT_TRUE(done_flag.load(memory_order::acquire));
            EXPECT_EQ(mtx, nullptr);
        }
    }

    /// @brief Stresses high-concurrency race windows between lock acquisition, suspension, and release.
    TEST(async_mutex_test, high_concurrency_race_window_stress)
    {
        // 1. Setup: 8 threads repeatedly acquiring and releasing the lock
        auto mtx = async_mutex{};
        constexpr auto thread_count = 8;
        constexpr auto iterations_per_thread = 500;
        auto shared_counter = int{0};

        auto worker_coroutine = [&mtx, &shared_counter]() -> task<void> {
            for (auto step = 0; step < iterations_per_thread; ++step)
            {
                auto guard = co_await mtx.scoped_lock();
                ++shared_counter;
            }
            co_return;
        };

        auto threads = vector<tempest::thread>{};
        threads.reserve(thread_count);

        auto tasks = vector<task<void>>{};
        tasks.reserve(thread_count);

        // 2. Act
        for (auto thread_index = 0; thread_index < thread_count; ++thread_index)
        {
            tasks.push_back(worker_coroutine());
            threads.push_back(tempest::thread([&tasks, thread_index]() {
                tasks[static_cast<size_t>(thread_index)].resume();
            }));
        }

        for (auto& thread : threads)
        {
            thread.join();
        }

        // 3. Assert: Exclusivity was strictly maintained without lost wakeups or dropped updates
        EXPECT_EQ(shared_counter, thread_count * iterations_per_thread);
    }

    /// @brief Verifies compile-time ergonomics of scoped_lock_guard ensuring it cannot
    ///        be manually constructed from an unheld async_mutex, and can only be acquired
    ///        via co_await mtx.scoped_lock().
    TEST(async_mutex_test, scoped_lock_guard_ergonomics)
    {
        // 1. Assert: scoped_lock_guard cannot be constructed from an async_mutex lvalue or rvalue
        static_assert(!is_constructible_v<scoped_lock_guard, async_mutex&>,
                      "scoped_lock_guard must not be publicly constructible from async_mutex reference");
        static_assert(!is_constructible_v<scoped_lock_guard, async_mutex>,
                      "scoped_lock_guard must not be publicly constructible from async_mutex value");
        static_assert(!is_copy_constructible_v<scoped_lock_guard>,
                      "scoped_lock_guard must not be copy-constructible");
        static_assert(is_move_constructible_v<scoped_lock_guard>,
                      "scoped_lock_guard must be move-constructible");

        // 2. Setup & Act: Legitimate acquisition via co_await mtx.scoped_lock()
        auto mtx = async_mutex{};
        auto acquired = false;

        auto test_coro = [&mtx, &acquired]() -> task<void> {
            auto guard = co_await mtx.scoped_lock();
            acquired = true;
            co_return;
        };

        auto t = test_coro();
        t.resume();

        // 3. Assert
        EXPECT_TRUE(acquired);
    }

    // =========================================================================
    // SECTION: Coroutine Frame Cancellation & Waiter Lifetime Tests
    // =========================================================================

    /// @brief Verifies that destroying a coroutine task while suspended on mtx.lock()
    ///        unlinks cleanly without use-after-free or dangling pointers, and allows
    ///        subsequent tasks to acquire the lock.
    TEST(async_mutex_test, destroy_suspended_coroutine_lock)
    {
        // 1. Setup: Mutex is initially locked
        auto mtx = async_mutex{};
        EXPECT_TRUE(mtx.try_lock());

        auto cancelled_task_ran = false;
        auto subsequent_task_ran = false;

        {
            auto waiting_coro = [&mtx, &cancelled_task_ran]() -> task<void> {
                co_await mtx.lock();
                cancelled_task_ran = true;
                mtx.unlock();
                co_return;
            };

            auto t = waiting_coro();
            t.resume(); // Suspends because mtx is locked
            EXPECT_FALSE(cancelled_task_ran);

            // 2. Act: Destroy t while suspended
        }

        // Unlock mtx. The cancelled waiter must be skipped and retired cleanly.
        mtx.unlock();
        EXPECT_FALSE(cancelled_task_ran);

        // 3. Assert: Subsequent acquisition succeeds
        auto subsequent_coro = [&mtx, &subsequent_task_ran]() -> task<void> {
            co_await mtx.lock();
            subsequent_task_ran = true;
            mtx.unlock();
            co_return;
        };

        auto t2 = subsequent_coro();
        t2.resume();
        EXPECT_TRUE(subsequent_task_ran);
    }

    /// @brief Verifies that destroying a coroutine task while suspended on mtx.scoped_lock()
    ///        unlinks cleanly and preserves mutex integrity.
    TEST(async_mutex_test, destroy_suspended_coroutine_scoped_lock)
    {
        // 1. Setup: Mutex is initially locked
        auto mtx = async_mutex{};
        EXPECT_TRUE(mtx.try_lock());

        auto cancelled_task_ran = false;
        auto subsequent_task_ran = false;

        {
            auto waiting_coro = [&mtx, &cancelled_task_ran]() -> task<void> {
                auto guard = co_await mtx.scoped_lock();
                cancelled_task_ran = true;
                co_return;
            };

            auto t = waiting_coro();
            t.resume(); // Suspends because mtx is locked
            EXPECT_FALSE(cancelled_task_ran);

            // 2. Act: Destroy t while suspended
        }

        mtx.unlock();
        EXPECT_FALSE(cancelled_task_ran);

        // 3. Assert: Subsequent scoped_lock succeeds
        auto subsequent_coro = [&mtx, &subsequent_task_ran]() -> task<void> {
            auto guard = co_await mtx.scoped_lock();
            subsequent_task_ran = true;
            co_return;
        };

        auto t2 = subsequent_coro();
        t2.resume();
        EXPECT_TRUE(subsequent_task_ran);
    }

    /// @brief Verifies that multiple interleaved cancelled and active waiters are properly
    ///        handled in FIFO order without lost wakeups or corruption.
    TEST(async_mutex_test, multiple_waiters_with_interleaved_cancellation)
    {
        // 1. Setup: Hold lock
        auto mtx = async_mutex{};
        EXPECT_TRUE(mtx.try_lock());

        auto order = vector<int>{};

        auto make_waiter = [&mtx, &order](int id) -> task<void> {
            auto guard = co_await mtx.scoped_lock();
            order.push_back(id);
            co_return;
        };

        auto t0 = make_waiter(0);
        auto t1 = make_waiter(1);
        auto t2 = make_waiter(2);
        auto t3 = make_waiter(3);

        t0.resume();
        t1.resume();
        t2.resume();
        t3.resume();

        // 2. Act: Cancel t1 and t3 before unlocking
        t1 = task<void>{};
        t3 = task<void>{};

        // Unlock mtx -> t0 should acquire and finish
        mtx.unlock();

        // 3. Assert: t0 executed, t2 executed, cancelled ones did not execute
        ASSERT_EQ(order.size(), 2U);
        EXPECT_EQ(order[0], 0);
        EXPECT_EQ(order[1], 2);
    }

    /// @brief Verifies that destroying an async_mutex with pending waiters retires them
    ///        safely without hanging or use-after-free when the coroutines are destroyed.
    TEST(async_mutex_test, destroy_async_mutex_with_pending_waiters)
    {
        // 1. Setup & Act: Create mutex and suspend tasks on it, then destroy mutex
        auto ran = false;
        auto t = task<void>{};

        {
            auto mtx = async_mutex{};
            EXPECT_TRUE(mtx.try_lock());

            auto waiting_coro = [&mtx, &ran]() -> task<void> {
                co_await mtx.lock();
                ran = true;
                mtx.unlock();
                co_return;
            };

            t = waiting_coro();
            t.resume(); // Suspends
            // mtx goes out of scope and is destroyed here
        }

        // 2. Act: Destroy t after mutex was destroyed
        t = task<void>{};

        // 3. Assert: Did not crash or hang
        EXPECT_FALSE(ran);
    }
} // namespace tempest::job::tests
