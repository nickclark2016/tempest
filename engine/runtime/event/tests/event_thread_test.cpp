#include <tempest/event_dispatcher.hpp>
#include <tempest/event_queue.hpp>
#include <tempest/thread.hpp>
#include <tempest/vector.hpp>

#include <gtest/gtest.h>

namespace
{
    struct thread_event
    {
        int thread_id;
        int sequence;
    };
} // namespace

TEST(event_thread, concurrent_enqueue_single_drain)
{
    tempest::event::event_queue<thread_event> queue;

    constexpr int num_threads = 4;
    constexpr int events_per_thread = 1000;

    tempest::vector<tempest::thread> threads;
    threads.reserve(num_threads);

    for (int tid = 0; tid < num_threads; ++tid)
    {
        threads.push_back(tempest::thread([&queue, tid]() {
            for (int idx = 0; idx < events_per_thread; ++idx)
            {
                queue.enqueue(thread_event{tid, idx});
            }
        }));
    }

    for (auto& worker : threads)
    {
        worker.join();
    }

    EXPECT_EQ(queue.size(), static_cast<size_t>(num_threads * events_per_thread));

    int total = 0;
    queue.drain([&](const thread_event&) {
        ++total;
    });

    EXPECT_EQ(total, num_threads * events_per_thread);
    EXPECT_EQ(queue.size(), 0U);
}

TEST(event_thread, concurrent_enqueue_during_drain)
{
    // Verifies that events enqueued during a drain end up in the next drain cycle
    // (Option A: swap-and-process).
    tempest::event::event_queue<thread_event> queue;

    constexpr int pre_events = 100;
    constexpr int concurrent_events = 200;

    for (int idx = 0; idx < pre_events; ++idx)
    {
        queue.enqueue(thread_event{0, idx});
    }

    // Start a thread that will enqueue events while main thread drains.
    tempest::thread producer([&queue]() {
        for (int idx = 0; idx < concurrent_events; ++idx)
        {
            queue.enqueue(thread_event{1, idx});
        }
    });

    // Drain the pre-enqueued events.
    int drained_first = 0;
    queue.drain([&](const thread_event&) {
        ++drained_first;
    });

    producer.join();

    // Some events from the producer may or may not have landed in the first drain
    // (depends on timing). But the total across both drains must equal pre + concurrent.
    int drained_second = 0;
    queue.drain([&](const thread_event&) {
        ++drained_second;
    });

    EXPECT_EQ(drained_first + drained_second, pre_events + concurrent_events);
}

TEST(event_thread, publish_fans_out_to_queue_from_threads)
{
    tempest::event::event_dispatcher<thread_event> dispatcher;
    tempest::event::event_queue<thread_event> queue;

    auto queue_handle = dispatcher.subscribe_queue(queue);

    constexpr int num_events = 500;
    constexpr int publisher_id = 99;

    // Publish from a background thread — the queue's enqueue is thread-safe.
    tempest::thread publisher([&dispatcher]() {
        for (int idx = 0; idx < num_events; ++idx)
        {
            dispatcher.publish(thread_event{publisher_id, idx});
        }
    });

    publisher.join();

    EXPECT_EQ(queue.size(), static_cast<size_t>(num_events));

    int count = 0;
    queue.drain([&](const thread_event&) {
        ++count;
    });
    EXPECT_EQ(count, num_events);

    static_cast<void>(dispatcher.unsubscribe_queue(queue_handle));
}
