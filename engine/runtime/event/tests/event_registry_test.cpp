#include <tempest/event_registry.hpp>

#include <gtest/gtest.h>

namespace
{
    struct reg_event_a
    {
        int value;
    };

    struct reg_event_b
    {
        float x;
    };

    constexpr int reg_value_a = 99;
    constexpr int reg_value_b = 10;
    constexpr int reg_value_c = 20;
    constexpr float reg_float_x = 1.0F;
} // namespace

TEST(event_registry, lazy_dispatcher_creation)
{
    tempest::event::event_registry registry;

    auto& disp_first = registry.dispatcher<reg_event_a>();
    auto& disp_second = registry.dispatcher<reg_event_a>();

    // Same dispatcher returned for the same type.
    EXPECT_EQ(&disp_first, &disp_second);
}

TEST(event_registry, distinct_dispatchers_per_type)
{
    tempest::event::event_registry registry;

    auto& disp_a = registry.dispatcher<reg_event_a>();
    auto& disp_b = registry.dispatcher<reg_event_b>();

    // Different types yield different dispatchers (different addresses).
    EXPECT_NE(static_cast<void*>(&disp_a), static_cast<void*>(&disp_b));
}

TEST(event_registry, publish_through_registry)
{
    tempest::event::event_registry registry;

    int received = 0;
    static_cast<void>(registry.dispatcher<reg_event_a>().subscribe(
        tempest::function<void(const reg_event_a&)>{[&](const reg_event_a& evt) {
            received = evt.value;
        }}));

    registry.dispatcher<reg_event_a>().publish(reg_event_a{reg_value_a});
    EXPECT_EQ(received, reg_value_a);
}

TEST(event_registry, full_round_trip_with_queue)
{
    tempest::event::event_registry registry;
    tempest::event::event_queue<reg_event_a> queue;

    auto& disp = registry.dispatcher<reg_event_a>();

    int immediate_sum = 0;
    static_cast<void>(disp.subscribe(tempest::function<void(const reg_event_a&)>{[&](const reg_event_a& evt) {
        immediate_sum += evt.value;
    }}));

    auto queue_handle = disp.subscribe_queue(queue);

    disp.publish(reg_event_a{reg_value_b});
    disp.publish(reg_event_a{reg_value_c});

    // Immediate listener received both events.
    EXPECT_EQ(immediate_sum, 30);

    // Queue buffered both events.
    int deferred_sum = 0;
    queue.drain([&](const reg_event_a& evt) {
        deferred_sum += evt.value;
    });
    EXPECT_EQ(deferred_sum, 30);

    static_cast<void>(disp.unsubscribe_queue(queue_handle));
}

TEST(event_registry, type_isolation)
{
    tempest::event::event_registry registry;

    int a_count = 0;
    int b_count = 0;

    static_cast<void>(registry.dispatcher<reg_event_a>().subscribe(
        tempest::function<void(const reg_event_a&)>{[&](const reg_event_a&) {
            ++a_count;
        }}));
    static_cast<void>(registry.dispatcher<reg_event_b>().subscribe(
        tempest::function<void(const reg_event_b&)>{[&](const reg_event_b&) {
            ++b_count;
        }}));

    registry.dispatcher<reg_event_a>().publish(reg_event_a{1});
    EXPECT_EQ(a_count, 1);
    EXPECT_EQ(b_count, 0);

    registry.dispatcher<reg_event_b>().publish(reg_event_b{reg_float_x});
    EXPECT_EQ(a_count, 1);
    EXPECT_EQ(b_count, 1);
}
