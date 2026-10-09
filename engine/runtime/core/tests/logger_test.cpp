#include <tempest/atomic.hpp>
#include <tempest/chrono.hpp>
#include <tempest/int.hpp>
#include <tempest/logger.hpp>
#include <tempest/mutex.hpp>
#include <tempest/string_view.hpp>
#include <tempest/thread.hpp>
#include <tempest/vector.hpp>

#include <gtest/gtest.h>

namespace
{
    class recording_log_sink final : public tempest::log_sink
    {
      public:
        using tempest::log_sink::log_sink;

        void do_log(const tempest::log_record& record) override
        {
            tempest::unique_lock lock(_mutex);
            _records.push_back(record);
        }

        [[nodiscard]] auto record_count() const -> size_t
        {
            tempest::shared_lock lock(_mutex);
            return _records.size();
        }

        [[nodiscard]] auto get_record(size_t index) const -> tempest::log_record
        {
            tempest::shared_lock lock(_mutex);
            return _records[index];
        }

        [[nodiscard]] auto copy_records() const -> tempest::vector<tempest::log_record>
        {
            tempest::shared_lock lock(_mutex);
            return _records;
        }

      private:
        mutable tempest::shared_mutex _mutex;
        tempest::vector<tempest::log_record> _records;
    };
} // namespace

// ============================================================================
// Multi-Sink Preservation Tests
// ============================================================================

/// @brief Verify that a logger with multiple registered sinks dispatches records to all sinks.
TEST(tempest_logger, default_logger_preserves_multiple_sinks)
{
    // 1. Setup: create multiple sinks and register them with a logger
    auto sink1 = recording_log_sink{};
    auto sink2 = recording_log_sink{};
    auto test_logger = tempest::logger{};

    test_logger.add_sink(sink1);
    test_logger.add_sink(sink2);

    // 2. Act: log messages of various levels
    test_logger.info("Message 1");
    test_logger.warn("Message 2");
    test_logger.error("Message 3");

    // 3. Assert: both sinks received all messages with correct metadata
    static constexpr size_t expected_count = 3;
    ASSERT_EQ(sink1.record_count(), expected_count);
    ASSERT_EQ(sink2.record_count(), expected_count);

    EXPECT_EQ(sink1.get_record(0).message, "Message 1");
    EXPECT_EQ(sink1.get_record(0).level, tempest::log_level::info);
    EXPECT_EQ(sink2.get_record(0).message, "Message 1");
    EXPECT_EQ(sink2.get_record(0).level, tempest::log_level::info);

    EXPECT_EQ(sink1.get_record(1).message, "Message 2");
    EXPECT_EQ(sink1.get_record(1).level, tempest::log_level::warn);
    EXPECT_EQ(sink2.get_record(1).message, "Message 2");
    EXPECT_EQ(sink2.get_record(1).level, tempest::log_level::warn);

    EXPECT_EQ(sink1.get_record(2).message, "Message 3");
    EXPECT_EQ(sink1.get_record(2).level, tempest::log_level::error);
    EXPECT_EQ(sink2.get_record(2).message, "Message 3");
    EXPECT_EQ(sink2.get_record(2).level, tempest::log_level::error);

    // 4. Act: remove one sink and log another message
    test_logger.remove_sink(sink1);
    test_logger.info("Message 4");

    // 5. Assert: only the remaining sink received Message 4
    EXPECT_EQ(sink1.record_count(), expected_count);
    EXPECT_EQ(sink2.record_count(), expected_count + 1);
    EXPECT_EQ(sink2.get_record(3).message, "Message 4");
}

// ============================================================================
// Monotonic Timestamp Tests
// ============================================================================

/// @brief Verify that log timestamps within a single thread are strictly non-decreasing and non-zero.
TEST(tempest_logger, monotonic_timestamps)
{
    // 1. Setup: initialize logger and recording sink
    auto sink = recording_log_sink{};
    auto test_logger = tempest::logger{};
    test_logger.add_sink(sink);

    static constexpr size_t log_iteration_count = 1000;

    // 2. Act: log sequential messages
    for (size_t i = 0; i < log_iteration_count; ++i)
    {
        test_logger.info("Monotonic timestamp check");
    }

    // 3. Assert: all timestamps are non-zero, monotonic, and wall clock base is positive
    ASSERT_EQ(sink.record_count(), log_iteration_count);
    EXPECT_GT(test_logger.wall_clock_base(), 0);

    const auto records = sink.copy_records();
    auto previous_timestamp_ns = uint64_t{0};

    for (const auto& rec : records)
    {
        EXPECT_GT(rec.timestamp_ns, 0ULL);
        EXPECT_GE(rec.timestamp_ns, previous_timestamp_ns);
        previous_timestamp_ns = rec.timestamp_ns;
    }
}

// ============================================================================
// Thread-Safety & High Contention Stress Tests
// ============================================================================

/// @brief Stress test with 8 concurrent worker threads logging while dynamic sinks are added/removed.
TEST(tempest_logger, dynamic_sinks_thread_safety)
{
    // 1. Setup: initialize logger with a persistent sink
    auto persistent_sink = recording_log_sink{};
    auto test_logger = tempest::logger{};
    test_logger.add_sink(persistent_sink);

    static constexpr size_t worker_thread_count = 8;
    static constexpr size_t iterations_per_worker = 1000;
    static constexpr size_t total_expected_logs = worker_thread_count * iterations_per_worker;

    auto start_flag = tempest::atomic<bool>{false};
    auto done_flag = tempest::atomic<bool>{false};

    // 2. Act: spawn 8 worker threads logging concurrently
    auto workers = tempest::vector<tempest::thread>{};
    workers.reserve(worker_thread_count);

    for (size_t thread_idx = 0; thread_idx < worker_thread_count; ++thread_idx)
    {
        workers.push_back(tempest::thread([&test_logger, &start_flag, iterations = iterations_per_worker]() {
            while (!start_flag.load(tempest::memory_order::acquire))
            {
                tempest::this_thread::yield();
            }

            for (size_t i = 0; i < iterations; ++i)
            {
                test_logger.info("Worker thread concurrent log message");
            }
        }));
    }

    // Spawn sink manager thread continuously adding and removing sinks
    auto sink_manager = tempest::thread([&test_logger, &start_flag, &done_flag]() {
        while (!start_flag.load(tempest::memory_order::acquire))
        {
            tempest::this_thread::yield();
        }

        auto dynamic_sink1 = recording_log_sink{};
        auto dynamic_sink2 = recording_log_sink{};

        while (!done_flag.load(tempest::memory_order::acquire))
        {
            test_logger.add_sink(dynamic_sink1);
            test_logger.add_sink(dynamic_sink2);
            tempest::this_thread::yield();

            test_logger.remove_sink(dynamic_sink1);
            test_logger.remove_sink(dynamic_sink2);
            tempest::this_thread::yield();
        }

        test_logger.remove_sink(dynamic_sink1);
        test_logger.remove_sink(dynamic_sink2);
    });

    // Release all threads simultaneously
    start_flag.store(true, tempest::memory_order::release);

    for (auto& worker : workers)
    {
        worker.join();
    }

    done_flag.store(true, tempest::memory_order::release);
    sink_manager.join();

    // 3. Assert: all worker log messages were captured by the persistent sink without loss or race
    EXPECT_EQ(persistent_sink.record_count(), total_expected_logs);
}
