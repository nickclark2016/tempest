#include <gtest/gtest.h>

#include <tempest/atomic.hpp>
#include <tempest/job/work_stealing_deque.hpp>
#include <tempest/thread.hpp>
#include <tempest/vector.hpp>

namespace tempest::job::tests
{
    // =========================================================================
    // SECTION: Single-Thread Owner Push / Pop (LIFO Order)
    // =========================================================================

    /// @brief Verifies single-threaded owner operations follow strict LIFO ordering,
    ///        track size accurately, and handle empty underflow gracefully.
    TEST(work_stealing_deque_test, single_thread_push_pop)
    {
        // 1. Setup: Construct a work-stealing deque with capacity 16
        auto deque = work_stealing_deque<int, 16>{};
        EXPECT_TRUE(deque.empty());
        EXPECT_EQ(deque.size(), 0u);

        // 2. Act: Push 5 items (10, 20, 30, 40, 50)
        for (auto val : {10, 20, 30, 40, 50})
        {
            EXPECT_TRUE(deque.try_push(val));
        }

        EXPECT_FALSE(deque.empty());
        EXPECT_EQ(deque.size(), 5u);

        // 3. Assert: Owner pops in strict LIFO order
        auto p5 = deque.pop();
        auto p4 = deque.pop();
        auto p3 = deque.pop();
        auto p2 = deque.pop();
        auto p1 = deque.pop();

        ASSERT_TRUE(p5.has_value());
        EXPECT_EQ(*p5, 50);

        ASSERT_TRUE(p4.has_value());
        EXPECT_EQ(*p4, 40);

        ASSERT_TRUE(p3.has_value());
        EXPECT_EQ(*p3, 30);

        ASSERT_TRUE(p2.has_value());
        EXPECT_EQ(*p2, 20);

        ASSERT_TRUE(p1.has_value());
        EXPECT_EQ(*p1, 10);

        // Underflow returns nullopt
        EXPECT_TRUE(deque.empty());
        EXPECT_FALSE(deque.pop().has_value());
    }

    // =========================================================================
    // SECTION: Multi-Thread Concurrent Thief Stealing (FIFO Order)
    // =========================================================================

    /// @brief Verifies concurrent thief threads stealing from top maintain FIFO
    ///        semantics and that every pushed element is popped or stolen exactly once.
    TEST(work_stealing_deque_test, multi_thread_concurrent_thief_steals)
    {
        // 1. Setup: Capacity 1024 deque
        auto deque = work_stealing_deque<int, 1024>{};
        constexpr auto total_items = 800;
        constexpr auto num_thieves = 4;

        auto total_stolen = atomic<int64_t>{0};
        auto stolen_checksum = atomic<int64_t>{0};
        auto stop_thieves = atomic<bool>{false};

        // 2. Act: Spawn thief threads
        auto thieves = vector<tempest::thread>{};
        thieves.reserve(num_thieves);

        for (auto t = 0; t < num_thieves; ++t)
        {
            thieves.push_back(tempest::thread{[&deque, &total_stolen, &stolen_checksum, &stop_thieves] {
                while (!stop_thieves.load(memory_order::acquire) || !deque.empty())
                {
                    auto item = deque.steal();
                    if (item.has_value())
                    {
                        total_stolen.fetch_add(1, memory_order::relaxed);
                        stolen_checksum.fetch_add(*item, memory_order::relaxed);
                    }
                    else
                    {
                        this_thread::yield();
                    }
                }
            }});
        }

        // Owner pushes items into the deque
        auto expected_checksum = int64_t{0};
        for (auto i = 1; i <= total_items; ++i)
        {
            expected_checksum += i;
            while (!deque.try_push(i))
            {
                this_thread::yield();
            }
        }

        // Owner also pops items concurrently
        auto owner_popped_count = 0;
        auto owner_checksum = int64_t{0};
        while (total_stolen.load(memory_order::acquire) + owner_popped_count < total_items)
        {
            auto item = deque.pop();
            if (item.has_value())
            {
                ++owner_popped_count;
                owner_checksum += *item;
            }
            else
            {
                this_thread::yield();
            }
        }

        stop_thieves.store(true, memory_order::release);
        for (auto& t : thieves)
        {
            t.join();
        }

        // 3. Assert: Exactly total_items consumed, with zero duplicates and zero lost items
        EXPECT_EQ(total_stolen.load(memory_order::relaxed) + owner_popped_count, total_items);
        EXPECT_EQ(stolen_checksum.load(memory_order::relaxed) + owner_checksum, expected_checksum);
    }

    // =========================================================================
    // SECTION: Bounded Capacity Saturation & Backpressure
    // =========================================================================

    /// @brief Verifies that work_stealing_deque enforces a strict bounded capacity,
    ///        rejecting pushes when saturated and resuming successful pushes once
    ///        elements are removed via owner pop or thief steal.
    TEST(work_stealing_deque_test, bounded_capacity_saturation)
    {
        // 1. Setup: Construct a work-stealing deque with capacity 64
        auto deque = work_stealing_deque<int, 64>{};
        EXPECT_EQ(deque.capacity(), 64u);
        EXPECT_EQ(deque.size(), 0u);

        // 2. Act & Assert: Push 64 items and verify each returns true
        for (auto index = 0; index < 64; ++index)
        {
            EXPECT_TRUE(deque.try_push(index));
        }
        EXPECT_EQ(deque.size(), 64u);

        // Pushing the 65th item returns false due to saturation
        EXPECT_FALSE(deque.try_push(64));
        EXPECT_EQ(deque.size(), 64u);

        // Owner pops an item; now try_push succeeds
        const auto popped_item = deque.pop();
        ASSERT_TRUE(popped_item.has_value());
        EXPECT_EQ(*popped_item, 63);
        EXPECT_EQ(deque.size(), 63u);
        EXPECT_TRUE(deque.try_push(100));
        EXPECT_EQ(deque.size(), 64u);
        EXPECT_FALSE(deque.try_push(101));

        // Thief steals an item; now try_push succeeds
        const auto stolen_item = deque.steal();
        ASSERT_TRUE(stolen_item.has_value());
        EXPECT_EQ(*stolen_item, 0);
        EXPECT_EQ(deque.size(), 63u);
        EXPECT_TRUE(deque.try_push(200));
        EXPECT_EQ(deque.size(), 64u);
        EXPECT_FALSE(deque.try_push(201));
    }
} // namespace tempest::job::tests
