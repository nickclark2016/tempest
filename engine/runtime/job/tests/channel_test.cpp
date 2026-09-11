#include <gtest/gtest.h>

#include <tempest/job/channel.hpp>
#include <tempest/job/job_system.hpp>
#include <tempest/job/spsc_channel.hpp>
#include <tempest/logger.hpp>
#include <tempest/profiler/session.hpp>

namespace tempest::job::tests
{
    namespace
    {
        auto producer_coro(channel<int, 2>& chan, int val, bool* completed) -> task<void>
        {
            co_await chan.push(val);
            *completed = true;
            co_return;
        }

        auto consumer_coro(channel<int, 2>& chan, int* out_val) -> task<void>
        {
            auto res = co_await chan.pop();
            *out_val = res.value();
            co_return;
        }
    } // namespace

    // =========================================================================
    // SECTION: MPMC Channel Tests (Backpressure & Guarded Wakeup)
    // =========================================================================

    /// @brief Verifies that pushing to a full channel yields the producer coroutine,
    ///        and that a consumer pop awakens the producer to complete its push.
    TEST(channel_test, full_channel_yield_and_consumer_wakeup)
    {
        // 1. Setup: Channel with capacity 2
        auto chan = channel<int, 2>{};
        EXPECT_TRUE(chan.try_push(10).has_value());
        EXPECT_TRUE(chan.try_push(20).has_value());

        // Channel is full; 3rd push should fail on try_push
        EXPECT_FALSE(chan.try_push(30).has_value());

        bool p3_done = false;
        auto p3 = producer_coro(chan, 30, &p3_done);

        // 2. Act: Producer tries to push and suspends
        p3.resume();
        EXPECT_FALSE(p3_done);
        EXPECT_FALSE(p3.is_ready());

        // Consumer pops one item
        int popped1 = 0;
        auto c1 = consumer_coro(chan, &popped1);
        c1.resume();

        // 3. Assert: Consumer read first item and awakened p3
        EXPECT_EQ(popped1, 10);
        EXPECT_TRUE(p3_done);
        EXPECT_TRUE(p3.is_ready());

        // Pop remaining items (20 and 30)
        int popped2 = 0;
        int popped3 = 0;
        auto c2 = consumer_coro(chan, &popped2);
        auto c3 = consumer_coro(chan, &popped3);
        c2.resume();
        c3.resume();

        EXPECT_EQ(popped2, 20);
        EXPECT_EQ(popped3, 30);
    }

    /// @brief Verifies guarded retry semantics when multiple suspended producers
    ///        compete for a newly freed slot upon consumer pop.
    TEST(channel_test, guarded_race_loss_retry_loop)
    {
        // 1. Setup: Capacity 2 channel filled with 2 items
        auto chan = channel<int, 2>{};
        EXPECT_TRUE(chan.try_push(100).has_value());
        EXPECT_TRUE(chan.try_push(150).has_value());

        bool p1_done = false;
        bool p2_done = false;

        auto p1 = [&chan, &p1_done]() -> task<void> {
            co_await chan.push(200);
            p1_done = true;
            co_return;
        };

        auto p2 = [&chan, &p2_done]() -> task<void> {
            co_await chan.push(300);
            p2_done = true;
            co_return;
        };

        auto t1 = p1();
        auto t2 = p2();

        // 2. Act: Both producers attempt to push and suspend
        t1.resume();
        t2.resume();

        EXPECT_FALSE(p1_done);
        EXPECT_FALSE(p2_done);

        // Consumer pops the initial item 100
        auto item1 = chan.try_pop();
        EXPECT_TRUE(item1.has_value());
        EXPECT_EQ(item1.value(), 100);

        // 3. Assert: Exactly one producer committed into the newly freed slot
        EXPECT_TRUE(p1_done || p2_done);
        EXPECT_FALSE(p1_done && p2_done);

        // Pop the second item
        auto item2 = chan.try_pop();
        EXPECT_TRUE(item2.has_value());

        // Now both producers can complete
        EXPECT_TRUE(p1_done && p2_done);
    }

    // =========================================================================
    // SECTION: SPSC Channel High-Throughput Tests
    // =========================================================================

    /// @brief Verifies lock-free single-producer single-consumer circular queue
    ///        throughput, ordering, and checksum integrity across thousands of items.
    TEST(channel_test, spsc_throughput_and_ordering)
    {
        // 1. Setup
        auto chan = spsc_channel<int, 64>{};
        constexpr int count = 5000;
        int64_t push_checksum = 0;
        int64_t pop_checksum = 0;

        // 2. Act
        for (int i = 0; i < count; ++i)
        {
            push_checksum += i;
            EXPECT_TRUE(chan.try_push(i));

            int val = 0;
            EXPECT_TRUE(chan.try_pop(val));
            EXPECT_EQ(val, i);
            pop_checksum += val;
        }

        // 3. Assert
        EXPECT_EQ(pop_checksum, push_checksum);
    }

    // =========================================================================
    // SECTION: Deterministic Single-Stepped Job System Channel Integration
    // =========================================================================

    /// @brief Verifies that under single-stepped execution (worker_count = 0),
    ///        awakened channel waiters are scheduled to the ready queue and only
    ///        commit when sys.step() is explicitly called.
    TEST(channel_test, deterministic_stepping_channel_yield)
    {
        // 1. Setup: Single-stepped job system harness
        auto log = logger{};
        auto prof = profiler::profiler_session{false};
        auto config = job_system_config{
            .performance_worker_count = 0,
            .efficiency_worker_count = 0,
        };
        auto sys = job_system{log, prof, config};

        auto chan = channel<int, 2>{sys};
        EXPECT_TRUE(chan.try_push(41).has_value());
        EXPECT_TRUE(chan.try_push(42).has_value()); // Fill the channel

        bool producer_committed = false;

        // 2. Act: Spawn producer task into job system
        [[maybe_unused]] auto prod_task = sys.async([&chan, &producer_committed]() -> task<void> {
            co_await chan.push(99);
            producer_committed = true;
            co_return;
        });

        // Step once: Producer executes, finds channel full, and yields
        sys.step();
        EXPECT_FALSE(producer_committed);

        // Pop item 41 from outside
        auto popped = chan.try_pop();
        EXPECT_TRUE(popped.has_value());
        EXPECT_EQ(popped.value(), 41);

        // Producer has been awakened into the ready queue, but has NOT stepped yet!
        EXPECT_FALSE(producer_committed);

        // 3. Assert: Step the job system -> producer commits
        sys.step();
        EXPECT_TRUE(producer_committed);

        auto popped2 = chan.try_pop();
        EXPECT_TRUE(popped2.has_value());
        EXPECT_EQ(popped2.value(), 42);

        auto popped3 = chan.try_pop();
        EXPECT_TRUE(popped3.has_value());
        EXPECT_EQ(popped3.value(), 99);
    }
} // namespace tempest::job::tests
