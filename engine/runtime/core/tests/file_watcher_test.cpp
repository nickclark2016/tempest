#include <tempest/file_watcher.hpp>
#include <tempest/files.hpp>
#include <tempest/filesystem.hpp>
#include <tempest/thread.hpp>
#include <tempest/vector.hpp>

#include <gtest/gtest.h>

namespace
{
    namespace fs = tempest::filesystem;

    /// @brief Helper that polls a file_watcher until at least min_events have arrived or timeout elapses.
    auto wait_for_events(
        tempest::file_watcher& watcher,
        size_t min_events,
        tempest::chrono::milliseconds timeout = tempest::chrono::milliseconds{2500})
        -> tempest::vector<tempest::file_watch_event>
    {
        auto events = tempest::vector<tempest::file_watch_event>{};
        const auto start_time = tempest::chrono::steady_clock::now();

        while (tempest::chrono::steady_clock::now() - start_time < timeout)
        {
            watcher.poll_events(events);
            if (events.size() >= min_events)
            {
                break;
            }
            tempest::this_thread::sleep_for(tempest::chrono::milliseconds{10});
        }

        return events;
    }
} // namespace

// ============================================================================
// Debounce and Burst Coalescing Tests
// ============================================================================

/// @brief Verifies that file modifications are detected and debounced into single events.
TEST(tempest_file_watcher, debounce_coalesces_burst_writes)
{
    // Setup: Create temporary directory and initialize file_watcher
    const auto temp_dir = fs::temp_directory_path() / "tempest_fw_debounce_test";
    fs::remove_all(temp_dir);
    ASSERT_TRUE(fs::create_directories(temp_dir));

    const auto test_file = temp_dir / "test.txt";
    const auto init_text = tempest::string_view{"initial file content"};
    ASSERT_TRUE(tempest::write_file_from_bytes(
        test_file,
        {reinterpret_cast<const tempest::byte*>(init_text.data()), init_text.size()}));

    tempest::this_thread::sleep_for(tempest::chrono::milliseconds{100});

    auto watcher = tempest::file_watcher{temp_dir};
    ASSERT_TRUE(watcher.is_running());
    static_cast<void>(watcher.poll_events());

    // Act: Rapidly issue 50 file write operations to "test.txt" within 50ms
    const auto start_burst = tempest::chrono::steady_clock::now();
    for (auto write_idx = 0; write_idx < 50; ++write_idx)
    {
        const auto payload = tempest::string_view{"burst payload data"};
        ASSERT_TRUE(tempest::write_file_from_bytes(
            test_file,
            {reinterpret_cast<const tempest::byte*>(payload.data()), payload.size()}));
    }
    const auto burst_duration = tempest::chrono::duration_cast<tempest::chrono::milliseconds>(
        tempest::chrono::steady_clock::now() - start_burst);
    EXPECT_LT(burst_duration.count(), 1000);

    // Mid-debounce check: within 50ms of burst finish, the 250ms debounce has not elapsed
    tempest::this_thread::sleep_for(tempest::chrono::milliseconds{50});
    const auto immediate_events = watcher.poll_events();
    EXPECT_EQ(immediate_events.size(), 0U);

    // Assert: Verify exactly 1 coalesced modified event is published after 250ms debounce
    const auto events = wait_for_events(watcher, 1, tempest::chrono::milliseconds{3000});
    ASSERT_EQ(events.size(), 1U);
    EXPECT_EQ(events[0].type, tempest::file_watch_event_type::modified);
    EXPECT_EQ(events[0].path.filename(), fs::path("test.txt"));

    // Verify no trailing duplicate events arrive afterwards
    tempest::this_thread::sleep_for(tempest::chrono::milliseconds{300});
    const auto trailing_events = watcher.poll_events();
    EXPECT_EQ(trailing_events.size(), 0U);

    // Cleanup
    fs::remove_all(temp_dir);
}

/// @brief Verifies that multiple separate files being written in bursts debounce independently.
TEST(tempest_file_watcher, multiple_files_debounce_independently)
{
    // Setup: Create two distinct files in temporary directory
    const auto temp_dir = fs::temp_directory_path() / "tempest_fw_multi_debounce";
    fs::remove_all(temp_dir);
    ASSERT_TRUE(fs::create_directories(temp_dir));

    const auto file_a = temp_dir / "file_a.txt";
    const auto file_b = temp_dir / "file_b.txt";
    const auto init_text = tempest::string_view{"baseline"};
    ASSERT_TRUE(tempest::write_file_from_bytes(
        file_a,
        {reinterpret_cast<const tempest::byte*>(init_text.data()), init_text.size()}));
    ASSERT_TRUE(tempest::write_file_from_bytes(
        file_b,
        {reinterpret_cast<const tempest::byte*>(init_text.data()), init_text.size()}));

    tempest::this_thread::sleep_for(tempest::chrono::milliseconds{100});

    auto watcher = tempest::file_watcher{temp_dir};
    ASSERT_TRUE(watcher.is_running());
    static_cast<void>(watcher.poll_events());

    // Act: Rapidly interleave writes to both files
    for (auto write_idx = 0; write_idx < 25; ++write_idx)
    {
        const auto payload_a = tempest::string_view{"alpha write"};
        const auto payload_b = tempest::string_view{"beta write"};
        ASSERT_TRUE(tempest::write_file_from_bytes(
            file_a,
            {reinterpret_cast<const tempest::byte*>(payload_a.data()), payload_a.size()}));
        ASSERT_TRUE(tempest::write_file_from_bytes(
            file_b,
            {reinterpret_cast<const tempest::byte*>(payload_b.data()), payload_b.size()}));
    }

    // Assert: Verify each file receives exactly 1 coalesced event after debounce
    const auto events = wait_for_events(watcher, 2, tempest::chrono::milliseconds{3000});
    ASSERT_EQ(events.size(), 2U);

    auto seen_a = false;
    auto seen_b = false;
    for (const auto& watch_event : events)
    {
        EXPECT_EQ(watch_event.type, tempest::file_watch_event_type::modified);
        if (watch_event.path.filename() == fs::path("file_a.txt"))
        {
            seen_a = true;
        }
        else if (watch_event.path.filename() == fs::path("file_b.txt"))
        {
            seen_b = true;
        }
    }
    EXPECT_TRUE(seen_a);
    EXPECT_TRUE(seen_b);

    // Cleanup
    fs::remove_all(temp_dir);
}

// ============================================================================
// File Creation, Removal and Rename Detection Tests
// ============================================================================

/// @brief Verifies that creating a new file publishes an added event.
TEST(tempest_file_watcher, file_added_detection)
{
    // Setup: Create empty directory and start file_watcher
    const auto temp_dir = fs::temp_directory_path() / "tempest_fw_add_test";
    fs::remove_all(temp_dir);
    ASSERT_TRUE(fs::create_directories(temp_dir));

    auto watcher = tempest::file_watcher{temp_dir};
    ASSERT_TRUE(watcher.is_running());
    static_cast<void>(watcher.poll_events());

    // Act: Create a brand new file
    const auto new_file = temp_dir / "created.txt";
    const auto payload = tempest::string_view{"brand new content"};
    ASSERT_TRUE(tempest::write_file_from_bytes(
        new_file,
        {reinterpret_cast<const tempest::byte*>(payload.data()), payload.size()}));

    // Assert: Wait for debounce and verify added event
    const auto events = wait_for_events(watcher, 1, tempest::chrono::milliseconds{3000});
    ASSERT_FALSE(events.empty());
    EXPECT_EQ(events[0].type, tempest::file_watch_event_type::added);
    EXPECT_EQ(events[0].path.filename(), fs::path("created.txt"));

    // Cleanup
    fs::remove_all(temp_dir);
}

/// @brief Verifies that removing an existing file publishes a removed event.
TEST(tempest_file_watcher, file_removed_detection)
{
    // Setup: Create directory with an existing file and initialize watcher
    const auto temp_dir = fs::temp_directory_path() / "tempest_fw_remove_test";
    fs::remove_all(temp_dir);
    ASSERT_TRUE(fs::create_directories(temp_dir));

    const auto file_to_delete = temp_dir / "delete_me.txt";
    const auto payload = tempest::string_view{"temporary payload"};
    ASSERT_TRUE(tempest::write_file_from_bytes(
        file_to_delete,
        {reinterpret_cast<const tempest::byte*>(payload.data()), payload.size()}));

    tempest::this_thread::sleep_for(tempest::chrono::milliseconds{100});

    auto watcher = tempest::file_watcher{temp_dir};
    ASSERT_TRUE(watcher.is_running());
    static_cast<void>(watcher.poll_events());

    // Act: Remove the file
    ASSERT_TRUE(fs::remove(file_to_delete));

    // Assert: Verify removed event received
    const auto events = wait_for_events(watcher, 1, tempest::chrono::milliseconds{3000});
    ASSERT_FALSE(events.empty());
    EXPECT_EQ(events[0].type, tempest::file_watch_event_type::removed);
    EXPECT_EQ(events[0].path.filename(), fs::path("delete_me.txt"));

    // Cleanup
    fs::remove_all(temp_dir);
}

/// @brief Verifies file rename detection and clean RAII thread shutdown.
TEST(tempest_file_watcher, rename_detection_and_clean_teardown)
{
    // Setup: Create temporary file "old_name.txt"
    const auto temp_dir = fs::temp_directory_path() / "tempest_fw_rename_test";
    fs::remove_all(temp_dir);
    ASSERT_TRUE(fs::create_directories(temp_dir));

    const auto old_file = temp_dir / "old_name.txt";
    const auto new_file = temp_dir / "new_name.txt";
    const auto payload = tempest::string_view{"rename test payload"};
    ASSERT_TRUE(tempest::write_file_from_bytes(
        old_file,
        {reinterpret_cast<const tempest::byte*>(payload.data()), payload.size()}));

    tempest::this_thread::sleep_for(tempest::chrono::milliseconds{100});

    auto watcher = tempest::make_unique<tempest::file_watcher>(temp_dir);
    ASSERT_TRUE(watcher->is_running());
    static_cast<void>(watcher->poll_events());

    // Act: Rename to "new_name.txt"
    ASSERT_TRUE(fs::rename(old_file, new_file));

    // Assert: Verify rename event received; destroy watcher and verify thread joins cleanly
    const auto events = wait_for_events(*watcher, 1, tempest::chrono::milliseconds{3000});
    ASSERT_FALSE(events.empty());

    auto found_rename = false;
    for (const auto& watch_event : events)
    {
        if (watch_event.type == tempest::file_watch_event_type::renamed)
        {
            EXPECT_EQ(watch_event.old_path.filename(), fs::path("old_name.txt"));
            EXPECT_EQ(watch_event.path.filename(), fs::path("new_name.txt"));
            found_rename = true;
            break;
        }
    }
    EXPECT_TRUE(found_rename);

    // Destroy watcher: RAII destructor signals stop, cancels pending I/O and joins worker thread cleanly
    watcher.reset();
    EXPECT_EQ(watcher, nullptr);

    // Cleanup
    fs::remove_all(temp_dir);
}

// ============================================================================
// RAII Lifecycle and Concurrency Safety Tests
// ============================================================================

/// @brief Verifies that rapid instantiation and destruction of file_watcher does not hang or leak threads.
TEST(tempest_file_watcher, rapid_raii_lifecycle_teardown)
{
    // Setup: Create a temporary directory
    const auto temp_dir = fs::temp_directory_path() / "tempest_fw_lifecycle_test";
    fs::remove_all(temp_dir);
    ASSERT_TRUE(fs::create_directories(temp_dir));

    // Act & Assert: Rapidly construct and immediately destruct watchers in succession
    for (auto cycle_idx = 0; cycle_idx < 10; ++cycle_idx)
    {
        auto watcher = tempest::file_watcher{temp_dir};
        EXPECT_TRUE(watcher.is_running());
        // Watcher destructs at end of loop scope
    }

    // Cleanup
    fs::remove_all(temp_dir);
}

/// @brief Verifies concurrent multi-producer push and single-consumer drain on mpsc_queue.
TEST(tempest_file_watcher, mpsc_queue_multithreaded_stress)
{
    // Setup: Initialize an mpsc_queue and spawn 4 concurrent producer threads
    auto queue = tempest::mpsc_queue<uint32_t>{};
    static constexpr size_t thread_count = 4;
    static constexpr size_t items_per_thread = 1000;

    auto threads = tempest::vector<tempest::thread>{};
    threads.reserve(thread_count);

    // Act: Each producer thread pushes items into the queue
    for (auto thread_idx = size_t{0}; thread_idx < thread_count; ++thread_idx)
    {
        threads.push_back(tempest::thread{[&queue, thread_idx]() {
            for (auto item_idx = size_t{0}; item_idx < items_per_thread; ++item_idx)
            {
                queue.push(static_cast<uint32_t>(thread_idx * 10000 + item_idx));
            }
        }});
    }

    // Join all producers
    for (auto& producer_thread : threads)
    {
        if (producer_thread.joinable())
        {
            producer_thread.join();
        }
    }

    // Assert: Consumer drains queue and verifies exact element count
    auto drained_items = tempest::vector<uint32_t>{};
    const auto drained_count = queue.drain(drained_items);

    EXPECT_EQ(drained_count, thread_count * items_per_thread);
    EXPECT_EQ(drained_items.size(), thread_count * items_per_thread);
    EXPECT_TRUE(queue.empty());
    EXPECT_EQ(queue.size(), 0U);
}
