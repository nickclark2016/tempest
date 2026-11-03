#include <tempest/array.hpp>
#include <tempest/atomic.hpp>
#include <tempest/file_watcher.hpp>
#include <tempest/files.hpp>
#include <tempest/filesystem.hpp>
#include <tempest/thread.hpp>
#include <tempest/utility.hpp>
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

// ============================================================================
// mpsc_queue Concurrency and Deadlock Avoidance Tests
// ============================================================================

/// @brief Verifies that self-assignment on mpsc_queue safely early-returns without deadlock or data loss.
TEST(tempest_file_watcher, mpsc_queue_self_move_assignment)
{
    // Setup: Initialize queue with sequential elements
    auto queue = tempest::mpsc_queue<uint32_t>{};
    static constexpr size_t item_count = 16;
    for (auto index = size_t{0}; index < item_count; ++index)
    {
        queue.push(static_cast<uint32_t>(index));
    }
    ASSERT_EQ(queue.size(), item_count);

    // Act: Self move assignment (guarded by if (this == &other))
    queue = tempest::move(queue);

    // Assert: Elements are preserved intact
    auto drained = tempest::vector<uint32_t>{};
    const auto drained_count = queue.drain(drained);
    EXPECT_EQ(drained_count, item_count);
    EXPECT_EQ(drained.size(), item_count);
    for (auto index = size_t{0}; index < item_count; ++index)
    {
        EXPECT_EQ(drained[index], static_cast<uint32_t>(index));
    }
}

/// @brief Verifies that concurrent reciprocal move assignments across two queues do not ABBA deadlock.
TEST(tempest_file_watcher, mpsc_queue_move_assignment_deadlock_stress)
{
    // Setup: Initialize two separate mpsc_queues
    auto queue_a = tempest::mpsc_queue<uint32_t>{};
    auto queue_b = tempest::mpsc_queue<uint32_t>{};
    static constexpr size_t iterations = 5000;

    auto threads = tempest::vector<tempest::thread>{};
    threads.reserve(2);

    // Act: Thread 1 repeatedly assigns A = move(B); Thread 2 repeatedly assigns B = move(A)
    threads.push_back(tempest::thread{[&queue_a, &queue_b]() {
        for (auto iteration = size_t{0}; iteration < iterations; ++iteration)
        {
            queue_a.push(static_cast<uint32_t>(iteration));
            queue_a = tempest::move(queue_b);
        }
    }});

    threads.push_back(tempest::thread{[&queue_a, &queue_b]() {
        for (auto iteration = size_t{0}; iteration < iterations; ++iteration)
        {
            queue_b.push(static_cast<uint32_t>(iteration));
            queue_b = tempest::move(queue_a);
        }
    }});

    // Join threads (if ABBA deadlock existed, this would hang indefinitely)
    for (auto& worker_thread : threads)
    {
        if (worker_thread.joinable())
        {
            worker_thread.join();
        }
    }

    // Assert: Both queues remain in a valid, drainable state
    auto drained_a = tempest::vector<uint32_t>{};
    auto drained_b = tempest::vector<uint32_t>{};
    static_cast<void>(queue_a.drain(drained_a));
    static_cast<void>(queue_b.drain(drained_b));
    EXPECT_TRUE(queue_a.empty());
    EXPECT_TRUE(queue_b.empty());
}

namespace
{
    struct queue_lifecycle_tracker
    {
        tempest::atomic<int32_t> alive_count{0};
        tempest::atomic<int32_t> construct_count{0};
        tempest::atomic<int32_t> destruct_count{0};
        tempest::atomic<int32_t> move_count{0};
    };

    struct queue_tracked_element
    {
        queue_lifecycle_tracker* tracker = nullptr;
        uint32_t payload = 0;

        queue_tracked_element() = default;

        explicit queue_tracked_element(queue_lifecycle_tracker* in_tracker, uint32_t in_payload)
            : tracker{in_tracker}, payload{in_payload}
        {
            if (tracker != nullptr)
            {
                tracker->alive_count.fetch_add(1, tempest::memory_order::relaxed);
                tracker->construct_count.fetch_add(1, tempest::memory_order::relaxed);
            }
        }

        ~queue_tracked_element()
        {
            if (tracker != nullptr)
            {
                tracker->alive_count.fetch_sub(1, tempest::memory_order::relaxed);
                tracker->destruct_count.fetch_add(1, tempest::memory_order::relaxed);
            }
        }

        queue_tracked_element(const queue_tracked_element&) = delete;
        auto operator=(const queue_tracked_element&) -> queue_tracked_element& = delete;

        queue_tracked_element(queue_tracked_element&& other) noexcept
            : tracker{tempest::exchange(other.tracker, nullptr)},
              payload{tempest::exchange(other.payload, 0)}
        {
            if (tracker != nullptr)
            {
                tracker->move_count.fetch_add(1, tempest::memory_order::relaxed);
            }
        }

        auto operator=(queue_tracked_element&& other) noexcept -> queue_tracked_element&
        {
            if (this == &other)
            {
                return *this;
            }

            if (tracker != nullptr)
            {
                tracker->alive_count.fetch_sub(1, tempest::memory_order::relaxed);
                tracker->destruct_count.fetch_add(1, tempest::memory_order::relaxed);
            }

            tracker = tempest::exchange(other.tracker, nullptr);
            payload = tempest::exchange(other.payload, 0);

            if (tracker != nullptr)
            {
                tracker->move_count.fetch_add(1, tempest::memory_order::relaxed);
            }

            return *this;
        }
    };
} // namespace

/// @brief Verifies that self-move assignment on empty and populated queues preserves invariants across repeated calls.
TEST(tempest_file_watcher, mpsc_queue_self_move_empty_and_repeated)
{
    // Setup: Initialize an empty queue and perform self-move
    auto empty_queue = tempest::mpsc_queue<uint32_t>{};
    EXPECT_TRUE(empty_queue.empty());
    EXPECT_EQ(empty_queue.size(), 0U);

    // Act: Self-move on empty queue
    empty_queue = tempest::move(empty_queue);

    // Assert: Empty queue remains safely valid and empty
    EXPECT_TRUE(empty_queue.empty());
    EXPECT_EQ(empty_queue.size(), 0U);

    // Setup: Populate a queue with sequential items
    auto populated_queue = tempest::mpsc_queue<uint32_t>{};
    static constexpr size_t item_count = 64;
    for (auto index = size_t{0}; index < item_count; ++index)
    {
        populated_queue.push(static_cast<uint32_t>(index * 3 + 1));
    }
    EXPECT_EQ(populated_queue.size(), item_count);

    // Act: Repeatedly self-move assign the populated queue
    for (auto iteration = size_t{0}; iteration < 10; ++iteration)
    {
        populated_queue = tempest::move(populated_queue);
    }

    // Assert: All original elements survive in identical FIFO order
    auto drained_items = tempest::vector<uint32_t>{};
    const auto drained_count = populated_queue.drain(drained_items);
    EXPECT_EQ(drained_count, item_count);
    EXPECT_EQ(drained_items.size(), item_count);
    for (auto index = size_t{0}; index < item_count; ++index)
    {
        EXPECT_EQ(drained_items[index], static_cast<uint32_t>(index * 3 + 1));
    }
    EXPECT_TRUE(populated_queue.empty());
}

/// @brief Verifies that move assignment properly destructs overwritten elements and transfers source items without leaks.
TEST(tempest_file_watcher, mpsc_queue_move_assignment_lifecycle_and_leak_oracle)
{
    // Setup: Initialize tracker and two queues with non-trivial tracked elements
    auto tracker = queue_lifecycle_tracker{};
    static constexpr size_t count_a = 50;
    static constexpr size_t count_b = 30;

    {
        auto queue_a = tempest::mpsc_queue<queue_tracked_element>{};
        auto queue_b = tempest::mpsc_queue<queue_tracked_element>{};

        for (auto index = size_t{0}; index < count_a; ++index)
        {
            queue_a.push(queue_tracked_element{&tracker, static_cast<uint32_t>(1000 + index)});
        }
        for (auto index = size_t{0}; index < count_b; ++index)
        {
            queue_b.push(queue_tracked_element{&tracker, static_cast<uint32_t>(2000 + index)});
        }

        EXPECT_EQ(tracker.alive_count.load(), static_cast<int32_t>(count_a + count_b));
        EXPECT_EQ(queue_a.size(), count_a);
        EXPECT_EQ(queue_b.size(), count_b);

        // Act: Move assign queue_b into queue_a, overwriting queue_a's original elements
        queue_a = tempest::move(queue_b);

        // Assert: queue_a now holds count_b items; queue_b is empty; count_a items were destroyed
        EXPECT_EQ(queue_a.size(), count_b);
        EXPECT_TRUE(queue_b.empty());
        EXPECT_EQ(queue_b.size(), 0U);
        EXPECT_EQ(tracker.alive_count.load(), static_cast<int32_t>(count_b));

        // Drain queue_a and inspect items
        auto drained = tempest::vector<queue_tracked_element>{};
        const auto drained_count = queue_a.drain(drained);
        EXPECT_EQ(drained_count, count_b);
        for (auto index = size_t{0}; index < count_b; ++index)
        {
            EXPECT_EQ(drained[index].payload, static_cast<uint32_t>(2000 + index));
        }

        EXPECT_EQ(tracker.alive_count.load(), static_cast<int32_t>(count_b));
    }

    // Assert: Scope exit destructs drained vector; all instances must be completely destructed with 0 leaks
    EXPECT_EQ(tracker.alive_count.load(), 0);
    EXPECT_EQ(tracker.construct_count.load(), tracker.destruct_count.load());
}

/// @brief Verifies that concurrent producer pushes do not corrupt data or deadlock while move assignments execute.
TEST(tempest_file_watcher, mpsc_queue_concurrent_push_while_moving)
{
    // Setup: Initialize two queues and control flags
    auto queue_a = tempest::mpsc_queue<uint32_t>{};
    auto queue_b = tempest::mpsc_queue<uint32_t>{};
    auto keep_running = tempest::atomic<bool>{true};

    static constexpr size_t producer_threads_count = 4;
    static constexpr size_t mover_threads_count = 2;
    static constexpr size_t consumer_threads_count = 2;
    static constexpr size_t operations_per_thread = 2000;

    auto threads = tempest::vector<tempest::thread>{};
    threads.reserve(producer_threads_count + mover_threads_count + consumer_threads_count);

    // Act: Producers rapidly push into queue_a and queue_b
    for (auto producer_idx = size_t{0}; producer_idx < producer_threads_count; ++producer_idx)
    {
        threads.push_back(tempest::thread{[&queue_a, &queue_b, &keep_running, producer_idx]() {
            for (auto step = size_t{0}; step < operations_per_thread && keep_running.load(tempest::memory_order::relaxed); ++step)
            {
                if ((producer_idx % 2) == 0)
                {
                    queue_a.push(static_cast<uint32_t>(step));
                }
                else
                {
                    queue_b.push(static_cast<uint32_t>(step));
                }
            }
        }});
    }

    // Act: Movers rapidly reciprocal-move assign queue_a and queue_b
    for (auto mover_idx = size_t{0}; mover_idx < mover_threads_count; ++mover_idx)
    {
        threads.push_back(tempest::thread{[&queue_a, &queue_b, &keep_running, mover_idx]() {
            for (auto step = size_t{0}; step < operations_per_thread && keep_running.load(tempest::memory_order::relaxed); ++step)
            {
                if ((mover_idx % 2) == 0)
                {
                    queue_a = tempest::move(queue_b);
                }
                else
                {
                    queue_b = tempest::move(queue_a);
                }
            }
        }});
    }

    // Act: Consumers periodically drain queues to maintain bounded queue sizes
    for (auto consumer_idx = size_t{0}; consumer_idx < consumer_threads_count; ++consumer_idx)
    {
        threads.push_back(tempest::thread{[&queue_a, &queue_b, &keep_running, consumer_idx]() {
            auto drain_buffer = tempest::vector<uint32_t>{};
            while (keep_running.load(tempest::memory_order::relaxed))
            {
                drain_buffer.clear();
                if ((consumer_idx % 2) == 0)
                {
                    static_cast<void>(queue_a.drain(drain_buffer));
                }
                else
                {
                    static_cast<void>(queue_b.drain(drain_buffer));
                }
                tempest::this_thread::yield();
            }
        }});
    }

    // Wait for producers and movers to finish
    for (auto thread_idx = size_t{0}; thread_idx < producer_threads_count + mover_threads_count; ++thread_idx)
    {
        if (threads[thread_idx].joinable())
        {
            threads[thread_idx].join();
        }
    }

    // Signal consumers to stop and join them
    keep_running.store(false, tempest::memory_order::relaxed);
    for (auto thread_idx = producer_threads_count + mover_threads_count; thread_idx < threads.size(); ++thread_idx)
    {
        if (threads[thread_idx].joinable())
        {
            threads[thread_idx].join();
        }
    }

    // Assert: Both queues drain cleanly without hanging or crashing
    auto final_drained_a = tempest::vector<uint32_t>{};
    auto final_drained_b = tempest::vector<uint32_t>{};
    static_cast<void>(queue_a.drain(final_drained_a));
    static_cast<void>(queue_b.drain(final_drained_b));
    EXPECT_TRUE(queue_a.empty());
    EXPECT_TRUE(queue_b.empty());
}

/// @brief Verifies that arbitrary pairwise move assignments across an array of queues cannot ABBA deadlock under multi-thread contention.
TEST(tempest_file_watcher, mpsc_queue_multi_thread_array_shuffle_deadlock_stress)
{
    // Setup: Initialize an array of independent queues
    static constexpr size_t queue_count = 8;
    static constexpr size_t worker_thread_count = 8;
    static constexpr size_t iterations_per_thread = 1000;

    auto queues = tempest::array<tempest::mpsc_queue<uint32_t>, queue_count>{};
    for (auto queue_idx = size_t{0}; queue_idx < queue_count; ++queue_idx)
    {
        queues[queue_idx].push(static_cast<uint32_t>(queue_idx));
    }

    auto threads = tempest::vector<tempest::thread>{};
    threads.reserve(worker_thread_count);

    // Act: Each worker thread performs arbitrary pairwise moves across the queue array
    for (auto worker_idx = size_t{0}; worker_idx < worker_thread_count; ++worker_idx)
    {
        threads.push_back(tempest::thread{[&queues, worker_idx]() {
            auto random_state = static_cast<uint32_t>(0x12345678u ^ (worker_idx * 0x9E3779B9u));
            auto next_random = [&random_state]() -> uint32_t {
                random_state = random_state * 1664525u + 1013904223u;
                return random_state;
            };

            for (auto step = size_t{0}; step < iterations_per_thread; ++step)
            {
                const auto source_idx = static_cast<size_t>(next_random() % queue_count);
                const auto target_idx = static_cast<size_t>(next_random() % queue_count);

                queues[target_idx].push(static_cast<uint32_t>(step));
                queues[target_idx] = tempest::move(queues[source_idx]);

                if ((step % 10) == 0)
                {
                    auto drain_buffer = tempest::vector<uint32_t>{};
                    static_cast<void>(queues[source_idx].drain(drain_buffer));
                }
            }
        }});
    }

    // Join all workers (any cycle in the wait-for graph would hang here)
    for (auto& worker_thread : threads)
    {
        if (worker_thread.joinable())
        {
            worker_thread.join();
        }
    }

    // Assert: All queues remain in a valid, emptyable state
    for (auto queue_idx = size_t{0}; queue_idx < queue_count; ++queue_idx)
    {
        auto final_drain = tempest::vector<uint32_t>{};
        static_cast<void>(queues[queue_idx].drain(final_drain));
        EXPECT_TRUE(queues[queue_idx].empty());
        EXPECT_EQ(queues[queue_idx].size(), 0U);
    }
}

/// @brief Verifies that move construction while concurrent producers push transfers elements atomically without dropping items.
TEST(tempest_file_watcher, mpsc_queue_concurrent_move_constructor_and_push)
{
    // Setup: Initialize shared source queue and total item count accounting
    auto source_queue = tempest::mpsc_queue<uint32_t>{};
    auto producers_done = tempest::atomic<bool>{false};

    static constexpr size_t producer_count = 4;
    static constexpr size_t items_per_producer = 1000;
    static constexpr size_t total_expected_items = producer_count * items_per_producer;

    auto threads = tempest::vector<tempest::thread>{};
    threads.reserve(producer_count);

    // Act: Producers push distinct items into source_queue
    for (auto producer_idx = size_t{0}; producer_idx < producer_count; ++producer_idx)
    {
        threads.push_back(tempest::thread{[&source_queue, producer_idx]() {
            for (auto item_idx = size_t{0}; item_idx < items_per_producer; ++item_idx)
            {
                const auto value = static_cast<uint32_t>(producer_idx * items_per_producer + item_idx);
                source_queue.push(value);
            }
        }});
    }

    // Act: Consumer thread repeatedly move-constructs from source_queue and drains
    auto collected_items = tempest::vector<uint32_t>{};
    auto consumer_thread = tempest::thread{[&source_queue, &producers_done, &collected_items]() {
        while (!producers_done.load(tempest::memory_order::relaxed) || !source_queue.empty())
        {
            // Move-construct new queue from source_queue
            auto snapshot_queue = tempest::mpsc_queue<uint32_t>{tempest::move(source_queue)};
            static_cast<void>(snapshot_queue.drain(collected_items));
            tempest::this_thread::yield();
        }
    }};

    // Join producers
    for (auto& producer_thread : threads)
    {
        if (producer_thread.joinable())
        {
            producer_thread.join();
        }
    }
    producers_done.store(true, tempest::memory_order::relaxed);

    // Join consumer
    if (consumer_thread.joinable())
    {
        consumer_thread.join();
    }

    // Final drain on source_queue to catch any items pushed after the last move
    static_cast<void>(source_queue.drain(collected_items));

    // Assert: Total collected items match exact total expected items with zero dropped items
    EXPECT_EQ(collected_items.size(), total_expected_items);
    EXPECT_TRUE(source_queue.empty());

    // Verify all item IDs from 0 to total_expected_items - 1 are present
    auto item_seen = tempest::vector<bool>{};
    item_seen.resize(total_expected_items, false);
    for (const auto value : collected_items)
    {
        ASSERT_LT(value, total_expected_items);
        EXPECT_FALSE(item_seen[value]);
        item_seen[value] = true;
    }
    for (auto index = size_t{0}; index < total_expected_items; ++index)
    {
        EXPECT_TRUE(item_seen[index]);
    }
}

