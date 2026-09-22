#include <gtest/gtest.h>

#include <tempest/atomic.hpp>
#include <tempest/int.hpp>
#include <tempest/job/mpsc_mailbox.hpp>
#include <tempest/memory.hpp>
#include <tempest/thread.hpp>
#include <tempest/utility.hpp>
#include <tempest/vector.hpp>

namespace tempest::job::tests
{
    namespace
    {
        struct lifecycle_tracker
        {
            static inline int constructor_count = 0;
            static inline int copy_count = 0;
            static inline int move_count = 0;
            static inline int destructor_count = 0;
            static inline int live_count = 0;

            static auto reset() noexcept -> void
            {
                constructor_count = 0;
                copy_count = 0;
                move_count = 0;
                destructor_count = 0;
                live_count = 0;
            }

            int value{0};

            lifecycle_tracker() noexcept
            {
                ++constructor_count;
                ++live_count;
            }

            explicit lifecycle_tracker(int val) noexcept : value{val}
            {
                ++constructor_count;
                ++live_count;
            }

            lifecycle_tracker(const lifecycle_tracker& other) noexcept : value{other.value}
            {
                ++copy_count;
                ++live_count;
            }

            lifecycle_tracker(lifecycle_tracker&& other) noexcept : value{other.value}
            {
                other.value = -1;
                ++move_count;
                ++live_count;
            }

            ~lifecycle_tracker() noexcept
            {
                ++destructor_count;
                --live_count;
            }

            auto operator=(const lifecycle_tracker& other) noexcept -> lifecycle_tracker&
            {
                value = other.value;
                return *this;
            }

            auto operator=(lifecycle_tracker&& other) noexcept -> lifecycle_tracker&
            {
                value = other.value;
                other.value = -1;
                return *this;
            }
        };
    } // namespace

    // =========================================================================
    // SECTION: Single Producer Push and Drain
    // =========================================================================

    /// @brief Verifies basic FIFO order, empty/capacity queries, and bulk drain count
    ///        under single-threaded producer-consumer usage.
    TEST(mpsc_mailbox_test, single_producer_push_and_drain)
    {
        // 1. Setup: Construct an mpsc_mailbox with capacity 8
        auto mailbox = mpsc_mailbox<int, 8>{};
        EXPECT_TRUE(mailbox.empty());
        EXPECT_EQ(mailbox.capacity(), 8u);

        // 2. Act: Push four sequential values
        EXPECT_EQ(mailbox.try_push(10), push_result::success);
        EXPECT_EQ(mailbox.try_push(20), push_result::success);
        EXPECT_EQ(mailbox.try_push(30), push_result::success);
        EXPECT_EQ(mailbox.try_push(40), push_result::success);

        EXPECT_FALSE(mailbox.empty());

        // 3. Assert: Drain all items and verify FIFO ordering
        auto received_items = vector<int>{};
        const auto drained_count = mailbox.drain([&received_items](int value) {
            received_items.push_back(value);
        });

        EXPECT_EQ(drained_count, 4u);
        EXPECT_TRUE(mailbox.empty());
        ASSERT_EQ(received_items.size(), 4u);
        EXPECT_EQ(received_items[0], 10);
        EXPECT_EQ(received_items[1], 20);
        EXPECT_EQ(received_items[2], 30);
        EXPECT_EQ(received_items[3], 40);

        // Subsequent drain on empty mailbox returns 0
        const auto empty_drain_count = mailbox.drain([]([[maybe_unused]] int value) {});
        EXPECT_EQ(empty_drain_count, 0u);
        EXPECT_TRUE(mailbox.empty());
    }

    // =========================================================================
    // SECTION: Capacity and Full Detection
    // =========================================================================

    /// @brief Verifies pushing past capacity returns push_result::full, and draining
    ///        recycles slots so subsequent pushes succeed.
    TEST(mpsc_mailbox_test, capacity_and_full_detection)
    {
        // 1. Setup: Construct an mpsc_mailbox with small capacity of 4
        auto mailbox = mpsc_mailbox<int, 4>{};
        EXPECT_EQ(mailbox.capacity(), 4u);
        EXPECT_TRUE(mailbox.empty());

        // 2. Act: Fill the mailbox to exact capacity
        EXPECT_EQ(mailbox.try_push(1), push_result::success);
        EXPECT_EQ(mailbox.try_push(2), push_result::success);
        EXPECT_EQ(mailbox.try_push(3), push_result::success);
        EXPECT_EQ(mailbox.try_push(4), push_result::success);
        EXPECT_FALSE(mailbox.empty());

        // Attempting to push past capacity must return push_result::full
        EXPECT_EQ(mailbox.try_push(5), push_result::full);
        EXPECT_EQ(mailbox.try_push(6), push_result::full);

        // 3. Assert: Drain mailbox and verify slot recycling on subsequent pushes
        auto received_items = vector<int>{};
        const auto initial_drain_count = mailbox.drain([&received_items](int value) {
            received_items.push_back(value);
        });
        EXPECT_EQ(initial_drain_count, 4u);
        EXPECT_TRUE(mailbox.empty());
        ASSERT_EQ(received_items.size(), 4u);
        EXPECT_EQ(received_items[0], 1);
        EXPECT_EQ(received_items[1], 2);
        EXPECT_EQ(received_items[2], 3);
        EXPECT_EQ(received_items[3], 4);

        // Now slots are recycled; pushing four new items must succeed
        EXPECT_EQ(mailbox.try_push(50), push_result::success);
        EXPECT_EQ(mailbox.try_push(60), push_result::success);
        EXPECT_EQ(mailbox.try_push(70), push_result::success);
        EXPECT_EQ(mailbox.try_push(80), push_result::success);
        EXPECT_EQ(mailbox.try_push(90), push_result::full);

        received_items.clear();
        const auto recycled_drain_count = mailbox.drain([&received_items](int value) {
            received_items.push_back(value);
        });
        EXPECT_EQ(recycled_drain_count, 4u);
        EXPECT_TRUE(mailbox.empty());
        ASSERT_EQ(received_items.size(), 4u);
        EXPECT_EQ(received_items[0], 50);
        EXPECT_EQ(received_items[1], 60);
        EXPECT_EQ(received_items[2], 70);
        EXPECT_EQ(received_items[3], 80);
    }

    // =========================================================================
    // SECTION: Non-Trivial Type Lifecycle
    // =========================================================================

    /// @brief Verifies non-trivial objects are constructed, moved, and destructed
    ///        properly on push, drain, and mailbox destruction without leaking.
    TEST(mpsc_mailbox_test, non_trivial_type_lifecycle)
    {
        // 1. Setup: Reset lifecycle tracking counters
        lifecycle_tracker::reset();

        // 2. Act: Push elements and drain them
        {
            auto mailbox = mpsc_mailbox<lifecycle_tracker, 4>{};

            EXPECT_EQ(mailbox.try_push(lifecycle_tracker{100}), push_result::success);
            EXPECT_EQ(mailbox.try_push(lifecycle_tracker{200}), push_result::success);
            EXPECT_EQ(lifecycle_tracker::live_count, 2);

            auto drained_values = vector<int>{};
            const auto drained_count = mailbox.drain([&drained_values](lifecycle_tracker item) {
                drained_values.push_back(item.value);
            });

            EXPECT_EQ(drained_count, 2u);
            EXPECT_EQ(lifecycle_tracker::live_count, 0);
            ASSERT_EQ(drained_values.size(), 2u);
            EXPECT_EQ(drained_values[0], 100);
            EXPECT_EQ(drained_values[1], 200);
        }

        // 3. Assert: All instances are cleanly destructed
        EXPECT_EQ(lifecycle_tracker::live_count, 0);
        EXPECT_EQ(lifecycle_tracker::constructor_count + lifecycle_tracker::copy_count + lifecycle_tracker::move_count,
                  lifecycle_tracker::destructor_count);

        // Test destruction cleanup: mailbox destroyed with unconsumed elements
        lifecycle_tracker::reset();
        {
            auto mailbox = mpsc_mailbox<lifecycle_tracker, 4>{};
            EXPECT_EQ(mailbox.try_push(lifecycle_tracker{1}), push_result::success);
            EXPECT_EQ(mailbox.try_push(lifecycle_tracker{2}), push_result::success);
            EXPECT_EQ(mailbox.try_push(lifecycle_tracker{3}), push_result::success);
            EXPECT_EQ(lifecycle_tracker::live_count, 3);
        } // mailbox destructor must invoke drain and destruct all remaining items

        EXPECT_EQ(lifecycle_tracker::live_count, 0);
        EXPECT_EQ(lifecycle_tracker::constructor_count + lifecycle_tracker::copy_count + lifecycle_tracker::move_count,
                  lifecycle_tracker::destructor_count);
    }

    // =========================================================================
    // SECTION: Move-Only Types
    // =========================================================================

    /// @brief Verifies move-only types such as tempest::unique_ptr can be pushed
    ///        and drained with perfect forwarding.
    TEST(mpsc_mailbox_test, move_only_types)
    {
        // 1. Setup: Construct an mpsc_mailbox holding move-only tempest::unique_ptr
        auto mailbox = mpsc_mailbox<tempest::unique_ptr<int>, 8>{};
        EXPECT_TRUE(mailbox.empty());

        // 2. Act: Push move-only unique_ptr instances
        auto pointer_one = tempest::make_unique<int>(42);
        auto pointer_two = tempest::make_unique<int>(84);

        EXPECT_EQ(mailbox.try_push(tempest::move(pointer_one)), push_result::success);
        EXPECT_EQ(mailbox.try_push(tempest::move(pointer_two)), push_result::success);

        EXPECT_EQ(pointer_one.get(), nullptr);
        EXPECT_EQ(pointer_two.get(), nullptr);
        EXPECT_FALSE(mailbox.empty());

        // 3. Assert: Drain move-only items and verify contents
        auto extracted_values = vector<int>{};
        const auto drained_count = mailbox.drain([&extracted_values](tempest::unique_ptr<int> pointer) {
            ASSERT_NE(pointer.get(), nullptr);
            extracted_values.push_back(*pointer);
        });

        EXPECT_EQ(drained_count, 2u);
        EXPECT_TRUE(mailbox.empty());
        ASSERT_EQ(extracted_values.size(), 2u);
        EXPECT_EQ(extracted_values[0], 42);
        EXPECT_EQ(extracted_values[1], 84);
    }

    // =========================================================================
    // SECTION: Multi-Producer Concurrent Stress
    // =========================================================================

    /// @brief Verifies 16 concurrent producer threads pushing 100,000 items total
    ///        while 1 consumer thread drains concurrently, ensuring zero dropped or duplicate items.
    TEST(mpsc_mailbox_test, multi_producer_concurrent_stress)
    {
        // 1. Setup: Allocate mailbox on heap with capacity 2048
        constexpr auto num_producers = size_t{16};
        constexpr auto items_per_producer = size_t{6250};
        constexpr auto total_items = num_producers * items_per_producer; // 100,000 total

        auto mailbox = tempest::make_unique<mpsc_mailbox<int64_t, 2048>>();

        auto drained_count = size_t{0};
        auto consumed_checksum = int64_t{0};
        auto producers_done = atomic<bool>{false};
        auto seen_items = vector<uint8_t>(total_items, uint8_t{0});
        auto detected_duplicate = false;

        // 2. Act: Start single consumer thread
        auto consumer_thread = tempest::thread{[&mailbox, &producers_done, &drained_count, &consumed_checksum,
                                                &seen_items, &detected_duplicate] {
            while (!producers_done.load(memory_order::acquire) || !mailbox->empty())
            {
                mailbox->drain([&drained_count, &consumed_checksum, &seen_items,
                                &detected_duplicate](int64_t item_value) {
                    consumed_checksum += item_value;
                    ++drained_count;

                    if (item_value >= 0 && static_cast<size_t>(item_value) < total_items)
                    {
                        const auto index = static_cast<size_t>(item_value);
                        if (seen_items[index] != 0)
                        {
                            detected_duplicate = true;
                        }
                        seen_items[index] = 1;
                    }
                });
                this_thread::yield();
            }

            // Final bulk drain to catch any trailing committed items
            mailbox->drain([&drained_count, &consumed_checksum, &seen_items,
                            &detected_duplicate](int64_t item_value) {
                consumed_checksum += item_value;
                ++drained_count;

                if (item_value >= 0 && static_cast<size_t>(item_value) < total_items)
                {
                    const auto index = static_cast<size_t>(item_value);
                    if (seen_items[index] != 0)
                    {
                        detected_duplicate = true;
                    }
                    seen_items[index] = 1;
                }
            });
        }};

        // Spawn 16 concurrent producer threads
        auto producer_threads = vector<tempest::thread>{};
        producer_threads.reserve(num_producers);

        for (auto producer_index = size_t{0}; producer_index < num_producers; ++producer_index)
        {
            producer_threads.push_back(tempest::thread{[&mailbox, producer_index, items_per_producer] {
                const auto base_value = static_cast<int64_t>(producer_index * items_per_producer);
                for (auto item_index = size_t{0}; item_index < items_per_producer; ++item_index)
                {
                    const auto item_value = base_value + static_cast<int64_t>(item_index);
                    while (mailbox->try_push(item_value) == push_result::full)
                    {
                        this_thread::yield();
                    }
                }
            }});
        }

        // Wait for all producers to finish pushing
        for (auto& thread : producer_threads)
        {
            thread.join();
        }

        // Signal producers are done and wait for consumer thread to drain completely
        producers_done.store(true, memory_order::release);
        consumer_thread.join();

        // 3. Assert: Verify exact count, expected checksum, zero duplicates, and zero dropped items
        constexpr auto expected_checksum =
            static_cast<int64_t>(total_items) * static_cast<int64_t>(total_items - 1) / 2;

        EXPECT_EQ(drained_count, total_items);
        EXPECT_EQ(consumed_checksum, expected_checksum);
        EXPECT_FALSE(detected_duplicate);
        EXPECT_TRUE(mailbox->empty());

        auto missing_count = size_t{0};
        for (auto item_seen_flag : seen_items)
        {
            if (item_seen_flag == 0)
            {
                ++missing_count;
            }
        }
        EXPECT_EQ(missing_count, 0u);
    }
} // namespace tempest::job::tests
