#include <gtest/gtest.h>

#include <tempest/atomic.hpp>
#include <tempest/job/job_system.hpp>
#include <tempest/job/parallel_for.hpp>
#include <tempest/logger.hpp>
#include <tempest/profiler/session.hpp>
#include <tempest/vector.hpp>

namespace tempest::job::tests
{
    // =========================================================================
    // SECTION: Static Chunk Partitioning
    // =========================================================================

    /// @brief Verifies that parallel_for with static_chunk partitioning divides the range
    ///        correctly and processes every element without data races or missing indices.
    TEST(parallel_for_test, static_partitioning_correctness)
    {
        // 1. Setup: 4-worker job system and output array of 1000 elements
        auto log = logger{};
        auto prof = profiler::profiler_session{false};
        auto config = job_system_config{
            .performance_worker_count = 4,
            .efficiency_worker_count = 0,
        };
        auto sys = job_system{log, prof, config};

        constexpr auto count = 1000u;
        auto values = vector<int>{};
        values.resize(count, 0);

        // 2. Act: Execute parallel_for with static chunking
        auto run = [&]() -> task<void> {
            co_await sys.parallel_for<partitioner::static_chunk>(count, [&](size_t i) {
                values[i] = static_cast<int>(i * 2);
            });
        };
        auto t = run();
        t.resume();
        sys.wait_idle();

        // 3. Assert: Verify every element was updated
        ASSERT_TRUE(t.is_ready());
        for (auto i = 0u; i < count; ++i)
        {
            EXPECT_EQ(values[i], static_cast<int>(i * 2));
        }
    }

    // =========================================================================
    // SECTION: Guided Partitioning & Range Sum
    // =========================================================================

    /// @brief Verifies that guided partitioning dynamically balances chunks and
    ///        correctly calculates the cumulative sum over a large range.
    TEST(parallel_for_test, guided_partitioning_correctness)
    {
        // 1. Setup: 4-worker job system
        auto log = logger{};
        auto prof = profiler::profiler_session{false};
        auto config = job_system_config{
            .performance_worker_count = 4,
            .efficiency_worker_count = 0,
        };
        auto sys = job_system{log, prof, config};

        constexpr auto count = 5000u;
        auto total_sum = atomic<int64_t>{0};

        // 2. Act: Execute parallel_for with guided partitioning
        auto run = [&]() -> task<void> {
            co_await sys.parallel_for<partitioner::guided>(count, [&](size_t i) {
                total_sum.fetch_add(static_cast<int64_t>(i + 1), memory_order::relaxed);
            });
        };
        auto t = run();
        t.resume();
        sys.wait_idle();

        // 3. Assert: Sum 1..5000 = 5000 * 5001 / 2 = 12502500
        ASSERT_TRUE(t.is_ready());
        constexpr auto expected_sum = static_cast<int64_t>(count) * (count + 1) / 2;
        EXPECT_EQ(total_sum.load(memory_order::relaxed), expected_sum);
    }

    // =========================================================================
    // SECTION: Subrange Chunk Body Overload
    // =========================================================================

    /// @brief Verifies that parallel_for accepts a callable taking a range<size_t> chunk.
    TEST(parallel_for_test, subrange_chunk_body)
    {
        // 1. Setup: 4-worker job system
        auto log = logger{};
        auto prof = profiler::profiler_session{false};
        auto config = job_system_config{
            .performance_worker_count = 4,
            .efficiency_worker_count = 0,
        };
        auto sys = job_system{log, prof, config};

        constexpr auto count = 800u;
        auto values = vector<int>{};
        values.resize(count, 0);

        // 2. Act: Parallel for with chunk-based body
        auto run = [&]() -> task<void> {
            co_await sys.parallel_for<partitioner::static_chunk>(
                range<size_t>{0, count}, task_priority::normal, [&](range<size_t> chunk) {
                    for (auto i = chunk.first; i < chunk.last; ++i)
                    {
                        values[i] = 777;
                    }
                });
        };
        auto t = run();
        t.resume();
        sys.wait_idle();

        // 3. Assert: All elements filled
        ASSERT_TRUE(t.is_ready());
        for (auto i = 0u; i < count; ++i)
        {
            EXPECT_EQ(values[i], 777);
        }
    }
} // namespace tempest::job::tests
