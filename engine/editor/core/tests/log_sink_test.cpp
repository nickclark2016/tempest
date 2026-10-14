#include <tempest/editor_log_sink.hpp>

#include <tempest/atomic.hpp>
#include <tempest/chrono.hpp>
#include <tempest/int.hpp>
#include <tempest/intrusive_stack.hpp>
#include <tempest/logger.hpp>
#include <tempest/string_view.hpp>
#include <tempest/thread.hpp>
#include <tempest/vector.hpp>

#include <gtest/gtest.h>

namespace tempest::editor::tests
{
    // ============================================================================
    // Single-Thread Enqueue & Drain Tests
    // ============================================================================

    /// @brief Verify that log records enqueued in a single thread are drained in exact FIFO chronological order.
    TEST(editor_log_sink_test, single_thread_enqueue_and_drain_fifo)
    {
        // Setup: instantiate editor_log_sink
        auto sink = editor_log_sink{};
        EXPECT_EQ(sink.enqueued_count(), 0ULL);

        // Act: emit 10 distinct records with varying messages
        static constexpr size_t test_record_count = 10;
        const char* const sample_messages[test_record_count] = {
            "Message 0: Initializing pipeline",
            "Message 1: Loading shader asset",
            "Message 2: Binding vertex buffers",
            "Message 3: Dispatching compute worker",
            "Message 4: Synchronizing timeline semaphore",
            "Message 5: Acquiring swapchain surface",
            "Message 6: Profiler frame tick",
            "Message 7: Warning texture layout mismatch",
            "Message 8: Recovering render graph target",
            "Message 9: Frame presentation completed",
        };

        for (size_t record_idx = 0; record_idx < test_record_count; ++record_idx)
        {
            const auto record = log_record{
                .level = static_cast<log_level>(record_idx % 7),
                .timestamp_ns = 100'000ULL + record_idx * 5'000ULL,
                .thread_id = static_cast<uint32_t>(1000 + record_idx),
                .message = sample_messages[record_idx],
                .source = source_location::current(),
            };
            sink.log(record);
        }

        // Assert: exact 10 nodes returned, in correct FIFO order, messages and metadata match
        EXPECT_EQ(sink.enqueued_count(), test_record_count);

        auto* const drained_head = sink.drain_nodes();
        ASSERT_NE(drained_head, nullptr);

        auto* current_node = drained_head;
        auto inspected_count = size_t{0};

        while (current_node != nullptr)
        {
            ASSERT_LT(inspected_count, test_record_count);
            EXPECT_EQ(current_node->message(), sample_messages[inspected_count]);
            EXPECT_EQ(current_node->level, static_cast<log_level>(inspected_count % 7));
            EXPECT_EQ(current_node->timestamp_ns, 100'000ULL + inspected_count * 5'000ULL);
            EXPECT_EQ(current_node->thread_id, static_cast<uint32_t>(1000 + inspected_count));

            auto* const next_node = intrusive_mpsc_stack<log_entry_node>::next(current_node);
            log_entry_node::destroy(current_node);
            current_node = next_node;
            ++inspected_count;
        }

        EXPECT_EQ(inspected_count, test_record_count);
        EXPECT_EQ(sink.drain_nodes(), nullptr);
    }

    // ============================================================================
    // High-Contention Multi-Producer Stress Tests
    // ============================================================================

    /// @brief Verify that 16 concurrent worker threads flooding 10,000 records each (160,000 total) drop 0 records.
    TEST(editor_log_sink_test, concurrent_worker_flood_no_drop_before_drain)
    {
        // Setup: instantiate logger and attach editor_log_sink
        auto test_logger = logger{};
        auto sink = editor_log_sink{test_logger};

        static constexpr size_t worker_thread_count = 16;
        static constexpr size_t logs_per_worker = 10'000;
        static constexpr size_t total_expected_logs = worker_thread_count * logs_per_worker;

        auto start_flag = atomic<bool>{false};

        // Act: spawn 16 tempest::thread workers, each emitting 10,000 log records concurrently
        auto workers = vector<thread>{};
        workers.reserve(worker_thread_count);

        for (size_t worker_idx = 0; worker_idx < worker_thread_count; ++worker_idx)
        {
            workers.push_back(thread([&test_logger, &start_flag]() {
                while (!start_flag.load(memory_order::acquire))
                {
                    this_thread::yield();
                }

                for (size_t iteration = 0; iteration < logs_per_worker; ++iteration)
                {
                    test_logger.info("Concurrent worker log burst message");
                }
            }));
        }

        // Release all workers simultaneously to maximize thread contention
        start_flag.store(true, memory_order::release);

        for (auto& worker : workers)
        {
            worker.join();
        }

        // Assert: drain_nodes() yields exactly 160,000 nodes without corruption. Free all nodes.
        EXPECT_EQ(sink.enqueued_count(), total_expected_logs);

        auto* const drained_head = sink.drain_nodes();
        ASSERT_NE(drained_head, nullptr);

        auto drained_count = size_t{0};
        auto* current_node = drained_head;

        while (current_node != nullptr)
        {
            ++drained_count;
            EXPECT_EQ(current_node->message(), "Concurrent worker log burst message");
            auto* const next_node = intrusive_mpsc_stack<log_entry_node>::next(current_node);
            log_entry_node::destroy(current_node);
            current_node = next_node;
        }

        EXPECT_EQ(drained_count, total_expected_logs);
        EXPECT_EQ(sink.drain_nodes(), nullptr);
    }

    // ============================================================================
    // RAII Lifecycle & Logger Unregistration Tests
    // ============================================================================

    /// @brief Verify that destroying an attached editor_log_sink cleanly detaches it from the logger.
    TEST(editor_log_sink_test, raii_unregistration_on_destruction)
    {
        // Setup: instantiate logger. In an inner scope, create editor_log_sink attached to the logger
        auto test_logger = logger{};

        {
            auto scoped_sink = editor_log_sink{test_logger};
            test_logger.info("Message inside sink lifetime");
            EXPECT_EQ(scoped_sink.enqueued_count(), 1ULL);
            // Act: destroy editor_log_sink by exiting scope
        }

        // Assert: logger emits safely without fault or crash
        test_logger.trace("Post-destruction trace");
        test_logger.debug("Post-destruction debug");
        test_logger.info("Post-destruction info");
        test_logger.warn("Post-destruction warn");
        test_logger.error("Post-destruction error");
        test_logger.critical("Post-destruction critical");
        test_logger.fatal("Post-destruction fatal");
    }

    /// @brief Verify that destructing a sink with pending entries cleanly frees all nodes without leaks.
    TEST(editor_log_sink_test, destructor_drains_remaining_nodes_without_leak)
    {
        // Setup & Act: Create sink, populate it with records, and let it destruct without draining
        {
            auto scoped_sink = editor_log_sink{};
            for (size_t record_idx = 0; record_idx < 100; ++record_idx)
            {
                const auto record = log_record{
                    .level = log_level::debug,
                    .timestamp_ns = 500ULL + record_idx,
                    .thread_id = 1,
                    .message = "Unconsumed log message for leak check",
                    .source = source_location::current(),
                };
                scoped_sink.log(record);
            }
            EXPECT_EQ(scoped_sink.enqueued_count(), 100ULL);
        }
        // Assert: Destructor completed cleanly without leaks or crashes
    }

    /// @brief Verify that explicit attach and detach calls properly control which records are captured.
    TEST(editor_log_sink_test, explicit_attach_and_detach)
    {
        // Setup: instantiate logger and default-constructed sink
        auto test_logger = logger{};
        auto sink = editor_log_sink{};
        EXPECT_EQ(sink.enqueued_count(), 0ULL);

        // Act: attach sink and emit first message
        sink.attach(test_logger);
        test_logger.info("Attached message 1");
        EXPECT_EQ(sink.enqueued_count(), 1ULL);

        // Act: detach sink and emit second message
        sink.detach();
        test_logger.info("Detached message - should not reach sink");
        EXPECT_EQ(sink.enqueued_count(), 1ULL);

        // Act: re-attach sink and emit third message
        sink.attach(test_logger);
        test_logger.info("Attached message 2");
        EXPECT_EQ(sink.enqueued_count(), 2ULL);

        // Assert: drain nodes and verify only attached messages were enqueued
        auto* const drained_head = sink.drain_nodes();
        ASSERT_NE(drained_head, nullptr);

        auto* const node1 = drained_head;
        EXPECT_EQ(node1->message(), "Attached message 1");

        auto* const node2 = intrusive_mpsc_stack<log_entry_node>::next(node1);
        ASSERT_NE(node2, nullptr);
        EXPECT_EQ(node2->message(), "Attached message 2");

        EXPECT_EQ(intrusive_mpsc_stack<log_entry_node>::next(node2), nullptr);

        log_entry_node::destroy(node1);
        log_entry_node::destroy(node2);
    }
} // namespace tempest::editor::tests
