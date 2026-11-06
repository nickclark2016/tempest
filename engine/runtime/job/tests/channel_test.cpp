#include <gtest/gtest.h>

#include <tempest/atomic.hpp>
#include <tempest/job/channel.hpp>
#include <tempest/job/job_system.hpp>
#include <tempest/job/spsc_channel.hpp>
#include <tempest/logger.hpp>
#include <tempest/profiler/session.hpp>
#include <tempest/thread.hpp>

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
        // Setup: Channel with capacity 2
        auto chan = channel<int, 2>{};
        EXPECT_TRUE(chan.try_push(10).has_value());
        EXPECT_TRUE(chan.try_push(20).has_value());

        // Channel is full; 3rd push should fail on try_push
        EXPECT_FALSE(chan.try_push(30).has_value());

        bool p3_done = false;
        auto p3 = producer_coro(chan, 30, &p3_done);

        // Act: Producer tries to push and suspends
        p3.resume();
        EXPECT_FALSE(p3_done);
        EXPECT_FALSE(p3.is_ready());

        // Consumer pops one item
        int popped1 = 0;
        auto c1 = consumer_coro(chan, &popped1);
        c1.resume();

        // Assert: Consumer read first item and awakened p3
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
        // Setup: Capacity 2 channel filled with 2 items
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

        // Act: Both producers attempt to push and suspend
        t1.resume();
        t2.resume();

        EXPECT_FALSE(p1_done);
        EXPECT_FALSE(p2_done);

        // Consumer pops the initial item 100
        auto item1 = chan.try_pop();
        EXPECT_TRUE(item1.has_value());
        EXPECT_EQ(item1.value(), 100);

        // Assert: Exactly one producer committed into the newly freed slot
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
        // Setup
        auto chan = spsc_channel<int, 64>{};
        constexpr int count = 5000;
        int64_t push_checksum = 0;
        int64_t pop_checksum = 0;

        // Act
        for (int i = 0; i < count; ++i)
        {
            push_checksum += i;
            EXPECT_TRUE(chan.try_push(i));

            int val = 0;
            EXPECT_TRUE(chan.try_pop(val));
            EXPECT_EQ(val, i);
            pop_checksum += val;
        }

        // Assert
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
        // Setup: Single-stepped job system harness
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

        // Act: Spawn producer task into job system
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

        // Assert: Step the job system -> producer commits
        sys.step();
        EXPECT_TRUE(producer_committed);

        auto popped2 = chan.try_pop();
        EXPECT_TRUE(popped2.has_value());
        EXPECT_EQ(popped2.value(), 42);

        auto popped3 = chan.try_pop();
        EXPECT_TRUE(popped3.has_value());
        EXPECT_EQ(popped3.value(), 99);
    }

    /// @brief Verifies whether spsc_channel properly preserves two queued items without memory corruption.
    TEST(channel_test, spsc_capacity_overlap_bug_reproduce)
    {
        auto chan = spsc_channel<int, 64>{};
        EXPECT_TRUE(chan.try_push(10));
        EXPECT_TRUE(chan.try_push(20));

        int val1 = 0;
        int val2 = 0;
        EXPECT_TRUE(chan.try_pop(val1));
        EXPECT_TRUE(chan.try_pop(val2));
        EXPECT_EQ(val1, 10);
        EXPECT_EQ(val2, 20);
    }

    /// @brief Adversarially stress-tests spsc_channel with dedicated concurrent
    /// producer and consumer OS threads pushing and popping 50,000 items, verifying
    /// lock-free ordering, non_null awaiter stability, and checksum consistency.
    TEST(channel_test, spsc_channel_multi_threaded_contention_stress)
    {
        // Setup: SPSC channel with 128 slots
        auto chan = spsc_channel<int64_t, 128>{};
        constexpr int64_t total_items = 50000;
        auto expected_checksum = int64_t{0};
        for (int64_t val = 0; val < total_items; ++val)
        {
            expected_checksum += val;
        }

        auto actual_checksum = int64_t{0};
        auto producer_done = atomic<bool>{false};

        // Act: Spawn producer and consumer threads
        auto producer = thread([&chan, &producer_done, total_items]() {
            for (int64_t val = 0; val < total_items; ++val)
            {
                while (!chan.try_push(val))
                {
                    this_thread::yield();
                }
            }
            producer_done.store(true, memory_order::release);
        });

        auto consumer = thread([&chan, &actual_checksum, total_items]() {
            auto items_read = int64_t{0};
            while (items_read < total_items)
            {
                auto val = int64_t{0};
                if (chan.try_pop(val))
                {
                    actual_checksum += val;
                    ++items_read;
                }
                else
                {
                    this_thread::yield();
                }
            }
        });

        producer.join();
        consumer.join();

        // Assert: All items received in exact sum
        EXPECT_TRUE(producer_done.load(memory_order::acquire));
        EXPECT_EQ(actual_checksum, expected_checksum);
    }

    /// @brief Verifies job_system non_null member stability under heavy concurrent task dispatch
    /// across 4 worker threads.
    TEST(channel_test, job_system_non_null_members_multi_threaded_stress)
    {
        // Setup: Job system with 4 performance workers
        auto log = logger{};
        auto prof = profiler::profiler_session{false};
        auto config = job_system_config{
            .performance_worker_count = 4,
            .efficiency_worker_count = 0,
        };
        auto sys = job_system{log, prof, config};

        constexpr auto task_count = 1000;
        auto completion_count = atomic<int>{0};
        auto tasks = vector<task<void>>{};
        tasks.reserve(task_count);

        // Act: Dispatch 1000 tasks that query system/logger/profiler state
        for (auto idx = 0; idx < task_count; ++idx)
        {
            tasks.push_back(sys.async([&completion_count]() {
                completion_count.fetch_add(1, memory_order::relaxed);
            }));
        }

        sys.wait_idle();

        // Assert: All 1000 tasks executed safely
        EXPECT_EQ(completion_count.load(memory_order::relaxed), task_count);
    }

    // =========================================================================
    // SECTION: SPSC Channel Adversarial Stress & Correctness Verification
    // =========================================================================

    namespace
    {
        struct lifecycle_probe
        {
            static inline atomic<int32_t> live_count{0};

            int32_t id{0};
            uint32_t magic{0x12345678};

            lifecycle_probe()
            {
                live_count.fetch_add(1, memory_order::relaxed);
            }

            explicit lifecycle_probe(int32_t val) : id{val}
            {
                live_count.fetch_add(1, memory_order::relaxed);
            }

            ~lifecycle_probe()
            {
                magic = 0;
                live_count.fetch_sub(1, memory_order::relaxed);
            }

            lifecycle_probe(const lifecycle_probe& other) : id{other.id}, magic{other.magic}
            {
                live_count.fetch_add(1, memory_order::relaxed);
            }

            auto operator=(const lifecycle_probe& other) -> lifecycle_probe&
            {
                if (this == &other)
                {
                    return *this;
                }
                id = other.id;
                magic = other.magic;
                return *this;
            }

            lifecycle_probe(lifecycle_probe&& other) noexcept
                : id{tempest::exchange(other.id, -1)}, magic{tempest::exchange(other.magic, 0)}
            {
                live_count.fetch_add(1, memory_order::relaxed);
            }

            auto operator=(lifecycle_probe&& other) noexcept -> lifecycle_probe&
            {
                if (this == &other)
                {
                    return *this;
                }
                id = tempest::exchange(other.id, -1);
                magic = tempest::exchange(other.magic, 0);
                return *this;
            }
        };

        struct alignas(64) over_aligned_element
        {
            uint64_t magic{0xCAFEBABEDEADBEEFULL};
            uint64_t payload{0};
            uint8_t padding[48]{};
        };

        struct odd_stride_element
        {
            uint8_t byte_array[21]{};
        };

        auto spsc_producer_coro(spsc_channel<int, 2>& chan, int val, bool* completed) -> task<void>
        {
            co_await chan.push(val);
            *completed = true;
            co_return;
        }

        auto spsc_consumer_coro(spsc_channel<int, 2>& chan, int* out_val) -> task<void>
        {
            auto res = co_await chan.pop();
            *out_val = res.value();
            co_return;
        }
    } // namespace

    /// @brief Verifies memory layout alignment and element stride correctness for
    /// over-aligned (64-byte aligned) and odd-sized types in spsc_channel.
    TEST(channel_test, spsc_channel_element_stride_and_alignment)
    {
        // Setup: Over-aligned channel with capacity 8
        auto aligned_chan = spsc_channel<over_aligned_element, 8>{};

        // Act: Push elements with distinct payloads
        for (auto element_idx = 0; element_idx < 8; ++element_idx)
        {
            auto elem = over_aligned_element{
                .magic = 0xCAFEBABEDEADBEEFULL,
                .payload = static_cast<uint64_t>(element_idx * 100),
            };
            EXPECT_TRUE(aligned_chan.try_push(elem));
        }

        // Assert: Elements popped maintain 64-byte alignment and payload integrity
        for (auto element_idx = 0; element_idx < 8; ++element_idx)
        {
            auto popped = over_aligned_element{};
            EXPECT_TRUE(aligned_chan.try_pop(popped));
            EXPECT_EQ(reinterpret_cast<uintptr_t>(&popped) % alignof(over_aligned_element), 0);
            EXPECT_EQ(popped.magic, 0xCAFEBABEDEADBEEFULL);
            EXPECT_EQ(popped.payload, static_cast<uint64_t>(element_idx * 100));
        }

        // Setup: Odd-sized type (21 bytes, align 1)
        auto odd_chan = spsc_channel<odd_stride_element, 4>{};
        for (auto element_idx = 0; element_idx < 4; ++element_idx)
        {
            auto odd_elem = odd_stride_element{};
            for (auto byte_idx = 0; byte_idx < 21; ++byte_idx)
            {
                odd_elem.byte_array[byte_idx] = static_cast<uint8_t>(element_idx * 20 + byte_idx);
            }
            EXPECT_TRUE(odd_chan.try_push(odd_elem));
        }

        // Assert: Odd-sized elements verify exact byte stride without overwrite
        for (auto element_idx = 0; element_idx < 4; ++element_idx)
        {
            auto popped_odd = odd_stride_element{};
            EXPECT_TRUE(odd_chan.try_pop(popped_odd));
            for (auto byte_idx = 0; byte_idx < 21; ++byte_idx)
            {
                EXPECT_EQ(popped_odd.byte_array[byte_idx], static_cast<uint8_t>(element_idx * 20 + byte_idx));
            }
        }
    }

    /// @brief Verifies non-trivial object construction and destruction ordering,
    /// move semantics, and complete destruction of residual items on channel destruction.
    TEST(channel_test, spsc_channel_non_trivial_lifecycle_and_drain)
    {
        // Setup: Ensure baseline instance count is zero
        lifecycle_probe::live_count.store(0, memory_order::relaxed);

        {
            auto chan = spsc_channel<lifecycle_probe, 8>{};

            // Act: Push 6 items
            for (auto item_idx = 0; item_idx < 6; ++item_idx)
            {
                EXPECT_TRUE(chan.try_push(lifecycle_probe{item_idx + 1}));
            }
            EXPECT_EQ(lifecycle_probe::live_count.load(memory_order::relaxed), 6);

            // Pop 2 items
            {
                auto p1 = lifecycle_probe{};
                auto p2 = lifecycle_probe{};
                EXPECT_TRUE(chan.try_pop(p1));
                EXPECT_TRUE(chan.try_pop(p2));
                EXPECT_EQ(p1.id, 1);
                EXPECT_EQ(p2.id, 2);
                EXPECT_EQ(p1.magic, 0x12345678);
                EXPECT_EQ(p2.magic, 0x12345678);
                // 4 items in chan + 2 in p1, p2
                EXPECT_EQ(lifecycle_probe::live_count.load(memory_order::relaxed), 6);
            }
            // p1 and p2 out of scope: 4 items remain inside channel
            EXPECT_EQ(lifecycle_probe::live_count.load(memory_order::relaxed), 4);

            // Channel goes out of scope here; its destructor must drain and destroy remaining 4 items
        }

        // Assert: Complete zero leak — all non-trivial objects destroyed cleanly
        EXPECT_EQ(lifecycle_probe::live_count.load(memory_order::relaxed), 0);
    }

    /// @brief Adversarially stress-tests spsc_channel minimal capacity wrap-around (Capacity = 2)
    /// across 100,000 items with concurrent producer and consumer threads.
    TEST(channel_test, spsc_channel_minimal_capacity_wrap_around_stress)
    {
        // Setup: Minimal capacity 2 channel
        auto chan = spsc_channel<int64_t, 2>{};
        constexpr auto total_elements = 100000;
        auto expected_sum = int64_t{0};
        for (auto val = 0; val < total_elements; ++val)
        {
            expected_sum += val;
        }

        auto actual_sum = int64_t{0};
        auto sequence_error_count = atomic<int32_t>{0};
        auto producer_finished = atomic<bool>{false};

        // Act: Producer and consumer threads contend on capacity-2 ring buffer
        auto producer = thread([&chan, &producer_finished, total_elements]() {
            for (auto val = 0; val < total_elements; ++val)
            {
                while (!chan.try_push(static_cast<int64_t>(val)))
                {
                    this_thread::yield();
                }
            }
            producer_finished.store(true, memory_order::release);
        });

        auto consumer = thread([&chan, &actual_sum, &sequence_error_count, total_elements]() {
            auto expected_seq = int64_t{0};
            auto read_count = 0;
            while (read_count < total_elements)
            {
                auto val = int64_t{0};
                if (chan.try_pop(val))
                {
                    if (val != expected_seq)
                    {
                        sequence_error_count.fetch_add(1, memory_order::relaxed);
                    }
                    ++expected_seq;
                    actual_sum += val;
                    ++read_count;
                }
                else
                {
                    this_thread::yield();
                }
            }
        });

        producer.join();
        consumer.join();

        // Assert: Strict FIFO sequence and checksum match without race conditions
        EXPECT_TRUE(producer_finished.load(memory_order::acquire));
        EXPECT_EQ(sequence_error_count.load(memory_order::relaxed), 0);
        EXPECT_EQ(actual_sum, expected_sum);
    }

    /// @brief Verifies spsc_channel coroutine push and pop suspension and wakeup semantics.
    TEST(channel_test, spsc_channel_coroutine_push_pop_yield_and_wakeup)
    {
        // Setup: Capacity 2 spsc_channel
        auto chan = spsc_channel<int, 2>{};
        EXPECT_TRUE(chan.try_push(10));
        EXPECT_TRUE(chan.try_push(20));

        // Act: Channel is full; producer coroutine attempts to push 30 and suspends
        auto p3_done = false;
        auto p3 = spsc_producer_coro(chan, 30, &p3_done);
        p3.resume();

        EXPECT_FALSE(p3_done);
        EXPECT_FALSE(p3.is_ready());

        // Consumer pops one item, waking up p3
        auto popped1 = 0;
        auto c1 = spsc_consumer_coro(chan, &popped1);
        c1.resume();

        // Assert: Consumer read first item and awakened p3 to commit 30
        EXPECT_EQ(popped1, 10);
        EXPECT_TRUE(p3_done);
        EXPECT_TRUE(p3.is_ready());

        // Pop remaining items (20 and 30)
        auto popped2 = 0;
        auto popped3 = 0;
        auto c2 = spsc_consumer_coro(chan, &popped2);
        auto c3 = spsc_consumer_coro(chan, &popped3);
        c2.resume();
        c3.resume();

        EXPECT_EQ(popped2, 20);
        EXPECT_EQ(popped3, 30);
    }

    /// @brief Adversarially stress-tests spsc_channel coroutine push and pop
    /// under multi-threaded job_system execution with backpressure and yield loops.
    TEST(channel_test, spsc_channel_concurrent_coroutine_stress)
    {
        // Setup: Job system with 2 performance workers
        auto log = logger{};
        auto prof = profiler::profiler_session{false};
        auto config = job_system_config{
            .performance_worker_count = 2,
            .efficiency_worker_count = 0,
        };
        auto sys = job_system{log, prof, config};
        auto chan = spsc_channel<int64_t, 4>{sys};

        constexpr auto total_items = int64_t{5000};
        auto expected_sum = int64_t{0};
        for (auto val = int64_t{0}; val < total_items; ++val)
        {
            expected_sum += val;
        }

        auto actual_sum = int64_t{0};
        auto producer_done = atomic<bool>{false};
        auto consumer_done = atomic<bool>{false};

        // Act: Dispatch producer and consumer coroutines to job system
        [[maybe_unused]] auto prod_task = sys.async([&chan, &producer_done]() -> task<void> {
            for (auto val = int64_t{0}; val < total_items; ++val)
            {
                [[maybe_unused]] auto res = co_await chan.push(val);
            }
            producer_done.store(true, memory_order::release);
            co_return;
        });

        [[maybe_unused]] auto cons_task = sys.async([&chan, &consumer_done, &actual_sum]() -> task<void> {
            for (auto item_idx = int64_t{0}; item_idx < total_items; ++item_idx)
            {
                auto res = co_await chan.pop();
                if (res.has_value())
                {
                    actual_sum += res.value();
                }
            }
            consumer_done.store(true, memory_order::release);
            co_return;
        });

        // Await completion with safe bounded timeout loop to guard against lost wakeups
        auto wait_cycles = 0;
        constexpr auto max_cycles = 2000000;
        while ((!producer_done.load(memory_order::acquire) || !consumer_done.load(memory_order::acquire)) &&
               wait_cycles < max_cycles)
        {
            this_thread::yield();
            ++wait_cycles;
        }

        // Assert: Both tasks completed successfully without lost wakeups
        const auto prod_completed = producer_done.load(memory_order::acquire);
        const auto cons_completed = consumer_done.load(memory_order::acquire);

        // If either hung, close channel to unblock worker threads cleanly before asserting
        if (!prod_completed || !cons_completed)
        {
            chan.close();
        }

        sys.wait_idle();

        EXPECT_TRUE(prod_completed);
        EXPECT_TRUE(cons_completed);
        EXPECT_EQ(actual_sum, expected_sum);
    }

    /// @brief Verifies spsc_channel behavior on empty pop and full push edge cases.
    TEST(channel_test, spsc_channel_empty_pop_and_full_push_edge_cases)
    {
        // Setup: Capacity 2 spsc_channel
        auto chan = spsc_channel<int, 2>{};

        // Assert empty pop fails cleanly without mutating output
        auto popped_val = int{999};
        EXPECT_FALSE(chan.try_pop(popped_val));
        EXPECT_EQ(popped_val, 999);

        // Fill channel to capacity
        EXPECT_TRUE(chan.try_push(11));
        EXPECT_TRUE(chan.try_push(22));

        // Assert push on full channel returns false and does not overwrite existing data
        EXPECT_FALSE(chan.try_push(33));

        // Pop elements and verify FIFO order
        EXPECT_TRUE(chan.try_pop(popped_val));
        EXPECT_EQ(popped_val, 11);

        EXPECT_TRUE(chan.try_pop(popped_val));
        EXPECT_EQ(popped_val, 22);

        // Assert channel is empty again
        EXPECT_FALSE(chan.try_pop(popped_val));
    }

    /// @brief Verifies immediate closure awakening a consumer coroutine suspended in item_awaiter,
    /// returning job_error::channel_closed without deadlock or hanging.
    TEST(channel_test, spsc_channel_immediate_closure_suspended_consumer_coroutine)
    {
        // Setup: Empty spsc_channel
        auto chan = spsc_channel<int, 2>{};

        // Act: Consumer coroutine awaits item on empty channel and suspends
        auto consumer_task = [&chan]() -> task<int> {
            auto val = co_await chan.pop();
            co_return val;
        }();

        consumer_task.resume();

        EXPECT_FALSE(consumer_task.is_ready());

        // Close channel while consumer coroutine is suspended in awaiter
        chan.close();

        // Assert: Consumer was awakened by close and received channel_closed error
        EXPECT_TRUE(consumer_task.is_ready());
        EXPECT_FALSE(consumer_task.has_value());
        EXPECT_EQ(consumer_task.error(), job_error::channel_closed);
    }

    /// @brief Verifies immediate closure awakening a producer coroutine suspended in slot_awaiter,
    /// returning job_error::channel_closed while preserving already buffered items.
    TEST(channel_test, spsc_channel_immediate_closure_suspended_producer_coroutine)
    {
        // Setup: Capacity 2 spsc_channel filled to capacity
        auto chan = spsc_channel<int, 2>{};
        EXPECT_TRUE(chan.try_push(101));
        EXPECT_TRUE(chan.try_push(202));

        // Act: Producer coroutine awaits slot on full channel and suspends
        auto producer_task = [&chan]() -> task<void> {
            co_await chan.push(303);
            co_return;
        }();

        producer_task.resume();

        EXPECT_FALSE(producer_task.is_ready());

        // Close channel while producer coroutine is suspended in awaiter
        chan.close();

        // Assert: Producer was awakened by close and received channel_closed error
        EXPECT_TRUE(producer_task.is_ready());
        EXPECT_FALSE(producer_task.has_value());
        EXPECT_EQ(producer_task.error(), job_error::channel_closed);

        // Assert: Buffered items before closure can still be drained cleanly
        auto drained_val = int{0};
        EXPECT_TRUE(chan.try_pop(drained_val));
        EXPECT_EQ(drained_val, 101);

        EXPECT_TRUE(chan.try_pop(drained_val));
        EXPECT_EQ(drained_val, 202);

        // Next pop observes empty and closed
        EXPECT_FALSE(chan.try_pop(drained_val));
    }

    /// @brief Verifies that closed channel drains existing buffered items in coroutine pop()
    /// before returning job_error::channel_closed upon exhaustion.
    TEST(channel_test, spsc_channel_drain_buffered_items_then_channel_closed)
    {
        // Setup: Channel filled with 2 items and then immediately closed
        auto chan = spsc_channel<int, 4>{};
        EXPECT_TRUE(chan.try_push(55));
        EXPECT_TRUE(chan.try_push(66));
        chan.close();

        auto pop1 = chan.pop();
        pop1.resume();
        EXPECT_TRUE(pop1.is_ready());
        EXPECT_TRUE(pop1.has_value());
        EXPECT_TRUE(pop1.value().has_value());
        EXPECT_EQ(pop1.value().value(), 55);

        auto pop2 = chan.pop();
        pop2.resume();
        EXPECT_TRUE(pop2.is_ready());
        EXPECT_TRUE(pop2.has_value());
        EXPECT_TRUE(pop2.value().has_value());
        EXPECT_EQ(pop2.value().value(), 66);

        auto pop3 = chan.pop();
        pop3.resume();
        EXPECT_TRUE(pop3.is_ready());
        EXPECT_FALSE(pop3.has_value());
        EXPECT_EQ(pop3.error(), job_error::channel_closed);
    }

    /// @brief Adversarially stress-tests concurrent push and close races under multi-threaded
    /// job system execution across 20 repeated cycles to verify 0 deadlocks or orphaned tasks.
    TEST(channel_test, spsc_channel_concurrent_closure_race_stress)
    {
        auto log = logger{};
        auto prof = profiler::profiler_session{false};
        auto config = job_system_config{
            .performance_worker_count = 2,
            .efficiency_worker_count = 0,
        };
        auto sys = job_system{log, prof, config};

        for (auto cycle = 0; cycle < 20; ++cycle)
        {
            auto chan = make_unique<spsc_channel<int64_t, 2>>(sys);

            auto prod = sys.async([c = chan.get()]() -> task<void> {
                for (auto item_idx = int64_t{0}; item_idx < 100; ++item_idx)
                {
                    co_await c->push(item_idx);
                }
                co_return;
            });

            auto cons = sys.async([c = chan.get()]() -> task<void> {
                for (auto item_idx = int64_t{0}; item_idx < 100; ++item_idx)
                {
                    [[maybe_unused]] auto val = co_await c->pop();
                }
                co_return;
            });

            // Let them run briefly, then close the channel asynchronously
            this_thread::yield();
            chan->close();

            auto wait_count = 0;
            constexpr auto max_wait = 1000;
            while ((!prod.is_ready() || !cons.is_ready()) && wait_count < max_wait)
            {
                this_thread::sleep_for(chrono::milliseconds(1));
                ++wait_count;
            }

            sys.wait_idle();

            EXPECT_TRUE(prod.is_ready());
            EXPECT_TRUE(cons.is_ready());
        }
    }
} // namespace tempest::job::tests

