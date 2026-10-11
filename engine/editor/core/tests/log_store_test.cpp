#include <tempest/log_store.hpp>

#include "mock_renderer.hpp"

#include <tempest/array.hpp>
#include <tempest/int.hpp>
#include <tempest/logger.hpp>
#include <tempest/source_location.hpp>
#include <tempest/string_view.hpp>
#include <tempest/vector.hpp>

#include <gtest/gtest.h>

namespace tempest::editor::tests
{
    // ============================================================================
    // Ring Buffer Capacity & Eviction Tests
    // ============================================================================

    /// @brief Verify that exceeding ring buffer capacity evicts oldest entries, updates sequence watermarks and dropped counters.
    TEST(log_store_test, ring_buffer_capacity_eviction_and_dropped_counters)
    {
        // 1. Setup: initialize log_store with fixed capacity of 100 entries and editor_log_sink
        constexpr auto test_capacity = size_t{100};
        constexpr auto total_records = size_t{250};
        constexpr auto expected_dropped = static_cast<uint64_t>(total_records - test_capacity);

        auto store = log_store{test_capacity};
        auto sink = editor_log_sink{};

        EXPECT_EQ(store.capacity(), test_capacity);
        EXPECT_EQ(store.size(), 0U);
        EXPECT_EQ(store.dropped_count(), 0ULL);
        EXPECT_EQ(store.oldest_sequence(), 0ULL);
        EXPECT_EQ(store.next_sequence(), 0ULL);

        // 2. Act: enqueue and drain 250 log records through the sink into the store
        for (auto record_idx = size_t{0}; record_idx < total_records; ++record_idx)
        {
            sink.log(log_record{
                .level = log_level::info,
                .timestamp_ns = 1'000ULL + record_idx * 10ULL,
                .thread_id = 1,
                .message = "Ring buffer capacity test record",
                .source = source_location::current(),
            });
        }

        const auto drained_count = store.drain(sink);

        // 3. Assert: exact capacity retained, oldest entries evicted, dropped count accurate
        EXPECT_EQ(drained_count, total_records);
        EXPECT_EQ(store.size(), test_capacity);
        EXPECT_EQ(store.dropped_count(), expected_dropped);
        EXPECT_EQ(store.oldest_sequence(), expected_dropped);
        EXPECT_EQ(store.next_sequence(), total_records);

        // Assert: evicted entries return nullptr, active entries return valid records
        EXPECT_EQ(store.get_entry(expected_dropped - 1), nullptr);
        const auto* const oldest_entry = store.get_entry(expected_dropped);
        ASSERT_NE(oldest_entry, nullptr);
        EXPECT_EQ(oldest_entry->sequence_number, expected_dropped);

        const auto* const newest_entry = store.get_entry(total_records - 1);
        ASSERT_NE(newest_entry, nullptr);
        EXPECT_EQ(newest_entry->sequence_number, total_records - 1);

        EXPECT_EQ(store.get_entry(total_records), nullptr);
    }

    // ============================================================================
    // Incremental Filter & Rebuild Equivalence Tests
    // ============================================================================

    /// @brief Verify that incremental filtering during ingestion produces bit-identical results to a full rebuild.
    TEST(log_store_test, incremental_matches_equal_full_rebuild)
    {
        // 1. Setup: configure store with capacity for 1,000 diverse records and pre-set filter for Warn+ and "network"
        constexpr auto test_capacity = size_t{2000};
        constexpr auto test_record_count = size_t{1000};

        auto store = log_store{test_capacity};
        auto sink = editor_log_sink{};

        // Filter: levels >= warn (trace, debug, info disabled) and search query "network"
        store.set_filter_level(log_level::trace, false);
        store.set_filter_level(log_level::debug, false);
        store.set_filter_level(log_level::info, false);
        store.set_filter_search("network");

        const auto sample_topics = array<string_view, 5>{
            "network connection established",
            "render pass shadow atlas",
            "physics rigid body collision",
            "network packet dropped on timeout",
            "audio stream buffer underrun",
        };

        // 2. Act: ingest 1,000 records of varying log levels and topics
        for (auto record_idx = size_t{0}; record_idx < test_record_count; ++record_idx)
        {
            const auto level = static_cast<log_level>(record_idx % 7);
            const auto topic = sample_topics[record_idx % 5];

            sink.log(log_record{
                .level = level,
                .timestamp_ns = 5'000ULL + record_idx * 50ULL,
                .thread_id = static_cast<uint32_t>(100 + (record_idx % 4)),
                .message = topic,
                .source = source_location::current(),
            });
        }

        store.drain(sink);

        // Copy incremental filter results
        const auto incremental_matches = store.filter().matched_sequences;
        const auto incremental_total = store.filter().total_count;
        const auto incremental_warn = store.filter().warn_count;
        const auto incremental_error = store.filter().error_count;
        const auto incremental_critical = store.filter().critical_count;
        const auto incremental_fatal = store.filter().fatal_count;

        ASSERT_FALSE(incremental_matches.empty());

        // 3. Act: force a full rebuild of the filter cache
        store.rebuild_filter();

        // Assert: incremental and rebuild results are bit-identical
        EXPECT_EQ(store.filter().matched_sequences.size(), incremental_matches.size());
        for (auto match_idx = size_t{0}; match_idx < incremental_matches.size(); ++match_idx)
        {
            EXPECT_EQ(store.filter().matched_sequences[match_idx], incremental_matches[match_idx]);
        }

        EXPECT_EQ(store.filter().total_count, incremental_total);
        EXPECT_EQ(store.filter().warn_count, incremental_warn);
        EXPECT_EQ(store.filter().error_count, incremental_error);
        EXPECT_EQ(store.filter().critical_count, incremental_critical);
        EXPECT_EQ(store.filter().fatal_count, incremental_fatal);
    }

    // ============================================================================
    // Soft Clear & Historical Watermark Tests
    // ============================================================================

    /// @brief Verify that clearing the store hides historical records, resets counters, and retains ring buffer allocation.
    TEST(log_store_test, clear_hides_historical_records_and_resets_counters)
    {
        // 1. Setup: ingest 50 records containing 10 warnings, 5 errors, and 35 info messages
        constexpr auto test_capacity = size_t{200};
        auto store = log_store{test_capacity};
        auto sink = editor_log_sink{};

        for (auto record_idx = size_t{0}; record_idx < 50; ++record_idx)
        {
            auto level = log_level::info;
            if (record_idx < 10)
            {
                level = log_level::warn;
            }
            else if (record_idx < 15)
            {
                level = log_level::error;
            }

            sink.log(log_record{
                .level = level,
                .timestamp_ns = 2'000ULL + record_idx * 10ULL,
                .thread_id = 1,
                .message = "Initial log burst",
                .source = source_location::current(),
            });
        }

        store.drain(sink);

        EXPECT_EQ(store.visible_count(), 50U);
        EXPECT_EQ(store.filter().warn_count, 10U);
        EXPECT_EQ(store.filter().error_count, 5U);
        EXPECT_EQ(store.filter().total_count, 50U);

        // 2. Act: clear the store
        store.clear();

        // Assert: visible count and filter counters reset, matches cleared, capacity retained
        EXPECT_EQ(store.visible_count(), 0U);
        EXPECT_EQ(store.filter().warn_count, 0U);
        EXPECT_EQ(store.filter().error_count, 0U);
        EXPECT_EQ(store.filter().total_count, 0U);
        EXPECT_TRUE(store.filter().matched_sequences.empty());
        EXPECT_EQ(store.capacity(), test_capacity);

        // Assert: get_entry for cleared records returns nullptr
        EXPECT_EQ(store.get_entry(0), nullptr);
        EXPECT_EQ(store.get_entry(49), nullptr);

        // 3. Act: ingest 10 new records (1 error and 9 info)
        for (auto record_idx = size_t{0}; record_idx < 10; ++record_idx)
        {
            const auto level = (record_idx == 0) ? log_level::error : log_level::info;
            sink.log(log_record{
                .level = level,
                .timestamp_ns = 10'000ULL + record_idx * 10ULL,
                .thread_id = 1,
                .message = "Post-clear log burst",
                .source = source_location::current(),
            });
        }

        store.drain(sink);

        // Assert: only post-clear records are visible and counted
        EXPECT_EQ(store.visible_count(), 10U);
        EXPECT_EQ(store.filter().error_count, 1U);
        EXPECT_EQ(store.filter().warn_count, 0U);
        EXPECT_EQ(store.filter().total_count, 10U);
        EXPECT_EQ(store.capacity(), test_capacity);

        // New records are accessible, cleared historical records remain inaccessible
        EXPECT_EQ(store.get_entry(49), nullptr);
        const auto* const new_entry = store.get_entry(50);
        ASSERT_NE(new_entry, nullptr);
        EXPECT_EQ(new_entry->sequence_number, 50ULL);
        EXPECT_EQ(new_entry->level(), log_level::error);
    }

    // ============================================================================
    // Ring Buffer Eviction Front-Trimming Tests
    // ============================================================================

    /// @brief Verify that matched sequence numbers are trimmed from the front when older records are evicted from the ring buffer.
    TEST(log_store_test, front_trimming_on_ring_eviction)
    {
        // 1. Setup: configure store of capacity 50 to match only warnings
        constexpr auto test_capacity = size_t{50};
        auto store = log_store{test_capacity};
        auto sink = editor_log_sink{};

        store.set_filter_level(log_level::trace, false);
        store.set_filter_level(log_level::debug, false);
        store.set_filter_level(log_level::info, false);
        store.set_filter_level(log_level::error, false);
        store.set_filter_level(log_level::critical, false);
        store.set_filter_level(log_level::fatal, false);

        // 2. Act: ingest 50 warnings filling the ring buffer entirely
        for (auto record_idx = size_t{0}; record_idx < test_capacity; ++record_idx)
        {
            sink.log(log_record{
                .level = log_level::warn,
                .timestamp_ns = 1'000ULL + record_idx,
                .thread_id = 1,
                .message = "Warning message",
                .source = source_location::current(),
            });
        }

        store.drain(sink);

        EXPECT_EQ(store.filter().matched_sequences.size(), 50U);
        for (auto record_idx = size_t{0}; record_idx < test_capacity; ++record_idx)
        {
            EXPECT_EQ(store.filter().matched_sequences[record_idx], record_idx);
        }

        // 3. Act: ingest 25 info records, causing eviction of sequences 0..24
        for (auto record_idx = size_t{0}; record_idx < 25; ++record_idx)
        {
            sink.log(log_record{
                .level = log_level::info,
                .timestamp_ns = 2'000ULL + record_idx,
                .thread_id = 1,
                .message = "Info message that should not match filter",
                .source = source_location::current(),
            });
        }

        store.drain(sink);

        // 4. Assert: matched sequences trimmed front prefix, remaining 25 entries span 25..49
        ASSERT_EQ(store.filter().matched_sequences.size(), 25U);
        EXPECT_EQ(store.filter().matched_sequences.front(), 25ULL);
        EXPECT_EQ(store.filter().matched_sequences.back(), 49ULL);

        for (auto match_idx = size_t{0}; match_idx < 25; ++match_idx)
        {
            EXPECT_EQ(store.filter().matched_sequences[match_idx], static_cast<uint64_t>(25 + match_idx));
        }
    }

    // ============================================================================
    // Search Query & Source Location Matching Tests
    // ============================================================================

    /// @brief Verify that case-insensitive search queries match message substrings and source location filenames.
    TEST(log_store_test, case_insensitive_and_source_matching)
    {
        // 1. Setup: instantiate log store and sink
        auto store = log_store{100};
        auto sink = editor_log_sink{};

        // 2. Act: emit records with varying case in messages and from a source filename containing 'renderer'
        // Record 0: "Renderer initialized" in message
        sink.log(log_record{
            .level = log_level::info,
            .timestamp_ns = 1'000ULL,
            .thread_id = 1,
            .message = "Renderer initialized",
            .source = source_location::current(),
        });

        // Record 1: "PRE_RENDER_PASS" in message
        sink.log(log_record{
            .level = log_level::info,
            .timestamp_ns = 2'000ULL,
            .thread_id = 1,
            .message = "PRE_RENDER_PASS",
            .source = source_location::current(),
        });

        // Record 2: emitted from mock_renderer.hpp (file_name contains 'renderer')
        emit_renderer_source_log(sink);

        // Record 3: unrelated message and source
        sink.log(log_record{
            .level = log_level::info,
            .timestamp_ns = 4'000ULL,
            .thread_id = 1,
            .message = "Physics collision solve step",
            .source = source_location::current(),
        });

        store.drain(sink);

        // 3. Act: set search query to lowercase "render"
        store.set_filter_search("render");

        // 4. Assert: records 0, 1, and 2 match, record 3 does not
        ASSERT_EQ(store.filter().matched_sequences.size(), 3U);
        EXPECT_EQ(store.filter().matched_sequences[0], 0ULL);
        EXPECT_EQ(store.filter().matched_sequences[1], 1ULL);
        EXPECT_EQ(store.filter().matched_sequences[2], 2ULL);
    }
} // namespace tempest::editor::tests
