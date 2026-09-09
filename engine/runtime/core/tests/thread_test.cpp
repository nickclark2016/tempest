#include <tempest/atomic.hpp>
#include <tempest/thread.hpp>
#include <tempest/utility.hpp>

#include <gtest/gtest.h>

TEST(thread, default_construct)
{
    tempest::thread t;
    ASSERT_FALSE(t.joinable());
}

TEST(thread, move_from_default_constructed)
{
    tempest::thread t1;
    tempest::thread t2 = tempest::move(t1);
    ASSERT_FALSE(t1.joinable());
    ASSERT_FALSE(t2.joinable());
}

TEST(thread, move_assign_from_default_constructed)
{
    tempest::thread t1;
    tempest::thread t2;
    t2 = tempest::move(t1);
    ASSERT_FALSE(t1.joinable());
    ASSERT_FALSE(t2.joinable());
}

TEST(thread, compute_value_in_thread)
{
    tempest::atomic<int> value{0};

    tempest::thread t([&value]() -> void { value.store(42); });
    t.join();

    ASSERT_EQ(value.load(), 42);
}

/// @brief Verify that tempest::this_thread::sleep_for suspends execution for at least the specified duration.
TEST(thread, sleep_for)
{
    // 1. Setup start time
    auto start = tempest::chrono::steady_clock::now();

    // 2. Act: sleep for 10 milliseconds
    tempest::this_thread::sleep_for(tempest::chrono::milliseconds{10});

    // 3. Assert: elapsed duration is at least 5 milliseconds (allowing OS timer scheduler tolerances)
    auto elapsed =
        tempest::chrono::duration_cast<tempest::chrono::milliseconds>(tempest::chrono::steady_clock::now() - start);
    ASSERT_GE(elapsed.count(), 5);
}
