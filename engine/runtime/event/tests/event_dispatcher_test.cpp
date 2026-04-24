#include <tempest/event_dispatcher.hpp>

#include <gtest/gtest.h>

namespace
{
    struct test_event
    {
        int value;
    };

    struct other_event
    {
        float x;
        float y;
    };

    // Named constants to avoid magic number diagnostics in tests.
    constexpr int test_value_a = 42;
    constexpr int test_value_b = 3;
    constexpr int test_multiplier = 10;
    constexpr int test_value_c = 5;
    constexpr int test_value_d = 10;
    constexpr int test_value_e = 7;
    constexpr int test_value_f = 8;
    constexpr float test_float_x = 1.0F;
    constexpr float test_float_y = 2.0F;
} // namespace

TEST(event_dispatcher, subscribe_and_publish)
{
    tempest::event::event_dispatcher<test_event> dispatcher;

    int received = 0;
    static_cast<void>(dispatcher.subscribe(tempest::function<void(const test_event&)>{[&](const test_event& evt) {
        received = evt.value;
    }}));

    dispatcher.publish(test_event{test_value_a});
    EXPECT_EQ(received, test_value_a);
}

TEST(event_dispatcher, multiple_listeners)
{
    tempest::event::event_dispatcher<test_event> dispatcher;

    int sum = 0;
    static_cast<void>(dispatcher.subscribe(tempest::function<void(const test_event&)>{[&](const test_event& evt) {
        sum += evt.value;
    }}));
    static_cast<void>(dispatcher.subscribe(tempest::function<void(const test_event&)>{[&](const test_event& evt) {
        sum += evt.value * test_multiplier;
    }}));

    dispatcher.publish(test_event{test_value_b});
    EXPECT_EQ(sum, 33); // 3 + 30
}

TEST(event_dispatcher, unsubscribe)
{
    tempest::event::event_dispatcher<test_event> dispatcher;

    int count = 0;
    auto handle = dispatcher.subscribe(tempest::function<void(const test_event&)>{[&](const test_event&) {
        ++count;
    }});

    dispatcher.publish(test_event{1});
    EXPECT_EQ(count, 1);

    bool removed = dispatcher.unsubscribe(handle);
    EXPECT_TRUE(removed);

    dispatcher.publish(test_event{2});
    EXPECT_EQ(count, 1); // listener was removed, count unchanged
}

TEST(event_dispatcher, unsubscribe_stale_handle)
{
    tempest::event::event_dispatcher<test_event> dispatcher;

    auto handle = dispatcher.subscribe(tempest::function<void(const test_event&)>{[](const test_event&) {}});
    static_cast<void>(dispatcher.unsubscribe(handle));

    // Second unsubscribe with the same handle should fail gracefully (stale generation).
    bool removed = dispatcher.unsubscribe(handle);
    EXPECT_FALSE(removed);
}

TEST(event_dispatcher, listener_count)
{
    tempest::event::event_dispatcher<test_event> dispatcher;

    EXPECT_EQ(dispatcher.listener_count(), 0U);

    auto handle_a = dispatcher.subscribe(tempest::function<void(const test_event&)>{[](const test_event&) {}});
    auto handle_b = dispatcher.subscribe(tempest::function<void(const test_event&)>{[](const test_event&) {}});
    EXPECT_EQ(dispatcher.listener_count(), 2U);

    static_cast<void>(dispatcher.unsubscribe(handle_a));
    EXPECT_EQ(dispatcher.listener_count(), 1U);

    static_cast<void>(dispatcher.unsubscribe(handle_b));
    EXPECT_EQ(dispatcher.listener_count(), 0U);
}

TEST(event_dispatcher, event_type_isolation)
{
    tempest::event::event_dispatcher<test_event> dispatcher_a;
    tempest::event::event_dispatcher<other_event> dispatcher_b;

    int a_count = 0;
    int b_count = 0;

    static_cast<void>(dispatcher_a.subscribe(tempest::function<void(const test_event&)>{[&](const test_event&) {
        ++a_count;
    }}));
    static_cast<void>(dispatcher_b.subscribe(tempest::function<void(const other_event&)>{[&](const other_event&) {
        ++b_count;
    }}));

    dispatcher_a.publish(test_event{1});
    EXPECT_EQ(a_count, 1);
    EXPECT_EQ(b_count, 0);

    dispatcher_b.publish(other_event{test_float_x, test_float_y});
    EXPECT_EQ(a_count, 1);
    EXPECT_EQ(b_count, 1);
}

TEST(event_dispatcher, publish_to_queue)
{
    tempest::event::event_dispatcher<test_event> dispatcher;
    tempest::event::event_queue<test_event> queue;

    auto queue_handle = dispatcher.subscribe_queue(queue);

    dispatcher.publish(test_event{test_value_e});
    dispatcher.publish(test_event{test_value_f});

    EXPECT_EQ(queue.size(), 2U);

    int sum = 0;
    queue.drain([&](const test_event& evt) {
        sum += evt.value;
    });

    EXPECT_EQ(sum, 15); // 7 + 8
    EXPECT_EQ(queue.size(), 0U);

    static_cast<void>(dispatcher.unsubscribe_queue(queue_handle));
}

TEST(event_dispatcher, unsubscribe_queue)
{
    tempest::event::event_dispatcher<test_event> dispatcher;
    tempest::event::event_queue<test_event> queue;

    auto queue_handle = dispatcher.subscribe_queue(queue);
    dispatcher.publish(test_event{1});
    EXPECT_EQ(queue.size(), 1U);

    queue.clear();
    static_cast<void>(dispatcher.unsubscribe_queue(queue_handle));

    dispatcher.publish(test_event{2});
    EXPECT_EQ(queue.size(), 0U); // queue no longer subscribed
}

TEST(event_dispatcher, listeners_and_queues_together)
{
    tempest::event::event_dispatcher<test_event> dispatcher;
    tempest::event::event_queue<test_event> queue;

    int listener_sum = 0;
    static_cast<void>(dispatcher.subscribe(tempest::function<void(const test_event&)>{[&](const test_event& evt) {
        listener_sum += evt.value;
    }}));

    auto queue_handle = dispatcher.subscribe_queue(queue);

    dispatcher.publish(test_event{test_value_c});
    dispatcher.publish(test_event{test_value_d});

    // Listener was invoked immediately.
    EXPECT_EQ(listener_sum, 15);

    // Queue has the same events buffered.
    int queue_sum = 0;
    queue.drain([&](const test_event& evt) {
        queue_sum += evt.value;
    });
    EXPECT_EQ(queue_sum, 15);

    static_cast<void>(dispatcher.unsubscribe_queue(queue_handle));
}
