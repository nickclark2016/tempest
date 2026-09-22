#include <gtest/gtest.h>

#include <tempest/array.hpp>
#include <tempest/atomic.hpp>
#include <tempest/int.hpp>
#include <tempest/job/idle_mask.hpp>
#include <tempest/thread.hpp>
#include <tempest/vector.hpp>

namespace tempest::job::tests
{
    // =========================================================================
    // SECTION: Basic Mark, Clear, and Query
    // =========================================================================

    /// @brief Verifies that marking and clearing idle workers operates correctly
    ///        across multi-word 64-bit boundaries up to 256 workers.
    TEST(idle_mask_test, basic_mark_and_clear)
    {
        // 1. Setup: Construct a scalable_idle_mask supporting 256 workers
        auto mask = scalable_idle_mask<256>{};

        for (auto index = size_t{0}; index < 256; ++index)
        {
            EXPECT_FALSE(mask.is_idle(index));
        }

        // 2. Act: Mark specific workers across word boundaries (word 0, 1, 2, 3)
        const auto test_indices = {size_t{0}, size_t{15}, size_t{63}, size_t{64}, size_t{100}, size_t{127},
                                   size_t{128}, size_t{200}, size_t{255}};

        for (auto index : test_indices)
        {
            mask.mark_idle(index);
            EXPECT_TRUE(mask.is_idle(index));
        }

        // Non-marked workers must remain false
        EXPECT_FALSE(mask.is_idle(1));
        EXPECT_FALSE(mask.is_idle(62));
        EXPECT_FALSE(mask.is_idle(65));
        EXPECT_FALSE(mask.is_idle(129));

        // 3. Assert: Clear idle workers and verify transition
        for (auto index : test_indices)
        {
            mask.clear_idle(index);
            EXPECT_FALSE(mask.is_idle(index));
        }
    }

    // =========================================================================
    // SECTION: Subrange Scanning Across Word Boundaries
    // =========================================================================

    /// @brief Verifies that find_idle finds the first idle worker within a given subrange,
    ///        correctly masking sub-word boundaries and word transitions.
    TEST(idle_mask_test, find_idle_subrange)
    {
        // 1. Setup: Mask with workers marked at 10, 62, 65, 70, 150
        auto mask = scalable_idle_mask<256>{};
        mask.mark_idle(10);
        mask.mark_idle(62);
        mask.mark_idle(65);
        mask.mark_idle(70);
        mask.mark_idle(150);

        // 2. Act & Assert: Query within word 0 [0, 50) -> should find 10
        auto found = mask.find_idle(0, 50);
        ASSERT_TRUE(found.has_value());
        EXPECT_EQ(*found, 10u);

        // Query within word 0 starting after 10 [11, 50) -> should be nullopt
        EXPECT_FALSE(mask.find_idle(11, 39).has_value());

        // Query crossing word 0 and word 1 [60, 65) -> should find 62
        found = mask.find_idle(60, 5);
        ASSERT_TRUE(found.has_value());
        EXPECT_EQ(*found, 62u);

        // Query starting in word 1 [64, 70) -> should find 65 (not 62)
        found = mask.find_idle(64, 6);
        ASSERT_TRUE(found.has_value());
        EXPECT_EQ(*found, 65u);

        // Query starting at 66 [66, 10) -> should find 70
        found = mask.find_idle(66, 10);
        ASSERT_TRUE(found.has_value());
        EXPECT_EQ(*found, 70u);

        // Query in word 2 [128, 64) -> should find 150
        found = mask.find_idle(128, 64);
        ASSERT_TRUE(found.has_value());
        EXPECT_EQ(*found, 150u);

        // Empty range query returns nullopt
        EXPECT_FALSE(mask.find_idle(10, 0).has_value());
    }

    // =========================================================================
    // SECTION: Atomic Claiming of First Idle Worker
    // =========================================================================

    /// @brief Verifies that claim_first_idle atomically claims and clears the first set bit.
    TEST(idle_mask_test, claim_first_idle_single_threaded)
    {
        // 1. Setup: Mark workers 5, 64, 130
        auto mask = scalable_idle_mask<256>{};
        mask.mark_idle(5);
        mask.mark_idle(64);
        mask.mark_idle(130);

        // 2. Act & Assert: Claim sequentially
        auto first_claim = mask.claim_first_idle(200);
        ASSERT_TRUE(first_claim.has_value());
        EXPECT_EQ(*first_claim, 5u);
        EXPECT_FALSE(mask.is_idle(5));

        auto second_claim = mask.claim_first_idle(200);
        ASSERT_TRUE(second_claim.has_value());
        EXPECT_EQ(*second_claim, 64u);
        EXPECT_FALSE(mask.is_idle(64));

        auto third_claim = mask.claim_first_idle(200);
        ASSERT_TRUE(third_claim.has_value());
        EXPECT_EQ(*third_claim, 130u);
        EXPECT_FALSE(mask.is_idle(130));

        // No more idle workers
        EXPECT_FALSE(mask.claim_first_idle(200).has_value());
    }

    // =========================================================================
    // SECTION: Multi-Threaded Concurrent Claiming Stress
    // =========================================================================

    /// @brief Verifies that concurrent threads calling claim_first_idle never double-claim
    ///        the same worker index.
    TEST(idle_mask_test, concurrent_claiming_stress)
    {
        // 1. Setup: 128 workers all marked idle
        constexpr auto worker_count = size_t{128};
        auto mask = scalable_idle_mask<256>{};
        for (auto index = size_t{0}; index < worker_count; ++index)
        {
            mask.mark_idle(index);
        }

        constexpr auto num_threads = size_t{8};
        auto claimed_counts = array<atomic<int>, worker_count>{};
        for (auto& count : claimed_counts)
        {
            count.store(0, memory_order::relaxed);
        }

        auto total_claimed = atomic<size_t>{0};

        // 2. Act: 8 threads concurrently claim idle workers
        auto threads = vector<tempest::thread>{};
        threads.reserve(num_threads);

        for (auto thread_idx = size_t{0}; thread_idx < num_threads; ++thread_idx)
        {
            threads.push_back(tempest::thread{[&mask, &claimed_counts, &total_claimed, worker_count] {
                while (true)
                {
                    auto claimed = mask.claim_first_idle(worker_count);
                    if (!claimed.has_value())
                    {
                        break;
                    }
                    claimed_counts[*claimed].fetch_add(1, memory_order::relaxed);
                    total_claimed.fetch_add(1, memory_order::relaxed);
                }
            }});
        }

        for (auto& thread : threads)
        {
            thread.join();
        }

        // 3. Assert: Exactly 128 workers claimed, each claimed exactly once
        EXPECT_EQ(total_claimed.load(memory_order::relaxed), worker_count);
        for (auto index = size_t{0}; index < worker_count; ++index)
        {
            EXPECT_EQ(claimed_counts[index].load(memory_order::relaxed), 1) << "Worker index " << index;
            EXPECT_FALSE(mask.is_idle(index));
        }
    }
} // namespace tempest::job::tests
