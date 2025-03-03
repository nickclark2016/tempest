#include <tempest/thread.hpp>

#include <future>

#include <gtest/gtest.h>

TEST(thread, default_construct)
{
    tempest::thread t;
    ASSERT_FALSE(t.joinable());
}

TEST(thread, move_from_default_constructed)
{
    tempest::thread t1;
    tempest::thread t2 = std::move(t1);
    ASSERT_FALSE(t1.joinable());
    ASSERT_FALSE(t2.joinable());
}

TEST(thread, move_assign_from_default_constructed)
{
    tempest::thread t1;
    tempest::thread t2;
    t2 = std::move(t1);
    ASSERT_FALSE(t1.joinable());
    ASSERT_FALSE(t2.joinable());
}

TEST(thread, compute_value_in_future)
{
    std::promise<int> value;
    std::future<int> value_fut = value.get_future();

    tempest::thread t([](std::promise<int> p) { p.set_value(42); }, std::move(value));

    ASSERT_EQ(value_fut.get(), 42);
    t.join();
}
