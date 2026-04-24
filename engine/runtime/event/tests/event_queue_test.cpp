#include <tempest/event_queue.hpp>

#include <gtest/gtest.h>

namespace
{
    struct queue_event
    {
        int id;
    };
} // namespace

TEST(event_queue, enqueue_and_drain)
{
    tempest::event::event_queue<queue_event> queue;

    queue.enqueue(queue_event{1});
    queue.enqueue(queue_event{2});
    queue.enqueue(queue_event{3});

    EXPECT_EQ(queue.size(), 3U);

    tempest::vector<int> collected;
    queue.drain([&](const queue_event& evt) {
        collected.push_back(evt.id);
    });

    ASSERT_EQ(collected.size(), 3U);
    EXPECT_EQ(collected[0], 1);
    EXPECT_EQ(collected[1], 2);
    EXPECT_EQ(collected[2], 3);
}

TEST(event_queue, drain_empties_queue)
{
    tempest::event::event_queue<queue_event> queue;

    queue.enqueue(queue_event{1});
    queue.drain([](const queue_event&) {});

    EXPECT_EQ(queue.size(), 0U);
}

TEST(event_queue, drain_on_empty_queue)
{
    tempest::event::event_queue<queue_event> queue;

    int count = 0;
    queue.drain([&](const queue_event&) {
        ++count;
    });

    EXPECT_EQ(count, 0);
}

TEST(event_queue, second_drain_returns_empty)
{
    tempest::event::event_queue<queue_event> queue;

    queue.enqueue(queue_event{1});
    queue.drain([](const queue_event&) {});

    int count = 0;
    queue.drain([&](const queue_event&) {
        ++count;
    });

    EXPECT_EQ(count, 0);
}

TEST(event_queue, clear)
{
    tempest::event::event_queue<queue_event> queue;

    queue.enqueue(queue_event{1});
    queue.enqueue(queue_event{2});
    queue.clear();

    EXPECT_EQ(queue.size(), 0U);
}

TEST(event_queue, enqueue_after_drain)
{
    tempest::event::event_queue<queue_event> queue;

    queue.enqueue(queue_event{1});
    queue.drain([](const queue_event&) {});

    queue.enqueue(queue_event{2});
    EXPECT_EQ(queue.size(), 1U);

    int received = 0;
    queue.drain([&](const queue_event& evt) {
        received = evt.id;
    });

    EXPECT_EQ(received, 2);
}
