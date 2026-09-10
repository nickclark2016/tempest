#include <gtest/gtest.h>

#include <tempest/job/async_mutex.hpp>
#include <tempest/job/task.hpp>
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
} // namespace tempest::job::tests
