#include <gtest/gtest.h>

#include <tempest/atomic.hpp>

#include <thread>

TEST(atomic_int8_test, value_construct)
{
    auto val = tempest::atomic<tempest::int8_t>{42};
    EXPECT_EQ(val.load(), 42);
    EXPECT_EQ(val.load(tempest::memory_order::relaxed), 42); // No other threads writing, so this should also be 42
    EXPECT_EQ(val.load(tempest::memory_order::acquire), 42);
    EXPECT_EQ(val.load(tempest::memory_order::seq_cst), 42);
    EXPECT_EQ(static_cast<tempest::int8_t>(val), 42);
}

TEST(atomic_int16_test, value_construct)
{
    const auto value = 12345;
    auto val = tempest::atomic<tempest::int16_t>{value};
    EXPECT_EQ(val.load(), value);
    EXPECT_EQ(val.load(tempest::memory_order::relaxed), value); // No other threads writing, so this hould be value
    EXPECT_EQ(val.load(tempest::memory_order::acquire), value);
    EXPECT_EQ(val.load(tempest::memory_order::seq_cst), value);
    EXPECT_EQ(static_cast<tempest::int16_t>(val), value);
}

TEST(atomic_int32_test, value_construct)
{
    const auto value = 123456789;
    auto val = tempest::atomic<tempest::int32_t>{value};
    EXPECT_EQ(val.load(), value);
    EXPECT_EQ(val.load(tempest::memory_order::relaxed), value); // No other threads writing, so this hould be value
    EXPECT_EQ(val.load(tempest::memory_order::acquire), value);
    EXPECT_EQ(val.load(tempest::memory_order::seq_cst), value);
    EXPECT_EQ(static_cast<tempest::int32_t>(val), value);
}

TEST(atomic_int64_test, value_construct)
{
    const auto value = 1234567890123456789LL;
    auto val = tempest::atomic<tempest::int64_t>{value};
    EXPECT_EQ(val.load(), value);
    EXPECT_EQ(val.load(tempest::memory_order::relaxed), value); // No other threads writing, so this hould be value
    EXPECT_EQ(val.load(tempest::memory_order::acquire), value);
    EXPECT_EQ(val.load(tempest::memory_order::seq_cst), value);
    EXPECT_EQ(static_cast<tempest::int64_t>(val), value);
}

TEST(atomic_int8_test, store_and_load)
{
    auto val = tempest::atomic<tempest::int8_t>{};
    const auto new_val = static_cast<tempest::int8_t>(-100);
    val.store(new_val);
    EXPECT_EQ(val.load(), new_val);
    EXPECT_EQ(val.load(tempest::memory_order::relaxed), new_val);
    EXPECT_EQ(val.load(tempest::memory_order::acquire), new_val);
    EXPECT_EQ(val.load(tempest::memory_order::seq_cst), new_val);
    EXPECT_EQ(static_cast<tempest::int8_t>(val), new_val);
}

TEST(atomic_int16_test, store_and_load)
{
    auto val = tempest::atomic<tempest::int16_t>{};
    const auto new_val = static_cast<tempest::int16_t>(-20000);
    val.store(new_val);
    EXPECT_EQ(val.load(), new_val);
    EXPECT_EQ(val.load(tempest::memory_order::relaxed), new_val);
    EXPECT_EQ(val.load(tempest::memory_order::acquire), new_val);
    EXPECT_EQ(val.load(tempest::memory_order::seq_cst), new_val);
    EXPECT_EQ(static_cast<tempest::int16_t>(val), new_val);
}

TEST(atomic_int32_test, store_and_load)
{
    auto val = tempest::atomic<tempest::int32_t>{};
    const auto new_val = static_cast<tempest::int32_t>(-2000000000);
    val.store(new_val);
    EXPECT_EQ(val.load(), new_val);
    EXPECT_EQ(val.load(tempest::memory_order::relaxed), new_val);
    EXPECT_EQ(val.load(tempest::memory_order::acquire), new_val);
    EXPECT_EQ(val.load(tempest::memory_order::seq_cst), new_val);
    EXPECT_EQ(static_cast<tempest::int32_t>(val), new_val);
}

TEST(atomic_int64_test, store_and_load)
{
    auto val = tempest::atomic<tempest::int64_t>{};
    const auto new_val = static_cast<tempest::int64_t>(-2000000000000000000LL);
    val.store(new_val);
    EXPECT_EQ(val.load(), new_val);
    EXPECT_EQ(val.load(tempest::memory_order::relaxed), new_val);
    EXPECT_EQ(val.load(tempest::memory_order::acquire), new_val);
    EXPECT_EQ(val.load(tempest::memory_order::seq_cst), new_val);
    EXPECT_EQ(static_cast<tempest::int64_t>(val), new_val);
}

TEST(atomic_int8_test, exchange)
{
    auto val = tempest::atomic<tempest::int8_t>{42};
    const auto new_val = static_cast<tempest::int8_t>(-100);
    const auto old_val = val.exchange(new_val);

    EXPECT_EQ(old_val, 42);
    EXPECT_EQ(val.load(), new_val);
}

TEST(atomic_int8_test, exchange_relaxed)
{
    auto val = tempest::atomic<tempest::int8_t>{42};
    const auto new_val = static_cast<tempest::int8_t>(-100);
    const auto old_val = val.exchange(new_val, tempest::memory_order::relaxed);

    EXPECT_EQ(old_val, 42);
    EXPECT_EQ(val.load(), new_val);
}

TEST(atomic_int8_test, exchange_acquire)
{
    auto val = tempest::atomic<tempest::int8_t>{42};
    const auto new_val = static_cast<tempest::int8_t>(-100);
    const auto old_val = val.exchange(new_val, tempest::memory_order::acquire);

    EXPECT_EQ(old_val, 42);
    EXPECT_EQ(val.load(), new_val);
}

TEST(atomic_int8_test, exchange_seq_cst)
{
    auto val = tempest::atomic<tempest::int8_t>{42};
    const auto new_val = static_cast<tempest::int8_t>(-100);
    const auto old_val = val.exchange(new_val, tempest::memory_order::seq_cst);

    EXPECT_EQ(old_val, 42);
    EXPECT_EQ(val.load(), new_val);
}

TEST(atomic_int16_test, exchange)
{
    auto val = tempest::atomic<tempest::int16_t>{12345};
    const auto new_val = static_cast<tempest::int16_t>(-20000);
    const auto old_val = val.exchange(new_val);

    EXPECT_EQ(old_val, 12345);
    EXPECT_EQ(val.load(), new_val);
}

TEST(atomic_int16_test, exchange_relaxed)
{
    auto val = tempest::atomic<tempest::int16_t>{12345};
    const auto new_val = static_cast<tempest::int16_t>(-20000);
    const auto old_val = val.exchange(new_val, tempest::memory_order::relaxed);

    EXPECT_EQ(old_val, 12345);
    EXPECT_EQ(val.load(), new_val);
}

TEST(atomic_int16_test, exchange_acquire)
{
    auto val = tempest::atomic<tempest::int16_t>{12345};
    const auto new_val = static_cast<tempest::int16_t>(-20000);
    const auto old_val = val.exchange(new_val, tempest::memory_order::acquire);

    EXPECT_EQ(old_val, 12345);
    EXPECT_EQ(val.load(), new_val);
}

TEST(atomic_int16_test, exchange_seq_cst)
{
    auto val = tempest::atomic<tempest::int16_t>{12345};
    const auto new_val = static_cast<tempest::int16_t>(-20000);
    const auto old_val = val.exchange(new_val, tempest::memory_order::seq_cst);

    EXPECT_EQ(old_val, 12345);
    EXPECT_EQ(val.load(), new_val);
}

TEST(atomic_int32_test, exchange)
{
    auto val = tempest::atomic<tempest::int32_t>{123456789};
    const auto new_val = static_cast<tempest::int32_t>(-2000000000);
    const auto old_val = val.exchange(new_val);

    EXPECT_EQ(old_val, 123456789);
    EXPECT_EQ(val.load(), new_val);
}

TEST(atomic_int32_test, exchange_relaxed)
{
    auto val = tempest::atomic<tempest::int32_t>{123456789};
    const auto new_val = static_cast<tempest::int32_t>(-2000000000);
    const auto old_val = val.exchange(new_val, tempest::memory_order::relaxed);

    EXPECT_EQ(old_val, 123456789);
    EXPECT_EQ(val.load(), new_val);
}

TEST(atomic_int32_test, exchange_acquire)
{
    auto val = tempest::atomic<tempest::int32_t>{123456789};
    const auto new_val = static_cast<tempest::int32_t>(-2000000000);
    const auto old_val = val.exchange(new_val, tempest::memory_order::acquire);

    EXPECT_EQ(old_val, 123456789);
    EXPECT_EQ(val.load(), new_val);
}

TEST(atomic_int32_test, exchange_seq_cst)
{
    auto val = tempest::atomic<tempest::int32_t>{123456789};
    const auto new_val = static_cast<tempest::int32_t>(-2000000000);
    const auto old_val = val.exchange(new_val, tempest::memory_order::seq_cst);

    EXPECT_EQ(old_val, 123456789);
    EXPECT_EQ(val.load(), new_val);
}

TEST(atomic_int64_test, exchange)
{
    auto val = tempest::atomic<tempest::int64_t>{1234567890123456789LL};
    const auto new_val = static_cast<tempest::int64_t>(-2000000000000000000LL);
    const auto old_val = val.exchange(new_val);

    EXPECT_EQ(old_val, 1234567890123456789LL);
    EXPECT_EQ(val.load(), new_val);
}

TEST(atomic_int64_test, exchange_relaxed)
{
    auto val = tempest::atomic<tempest::int64_t>{1234567890123456789LL};
    const auto new_val = static_cast<tempest::int64_t>(-2000000000000000000LL);
    const auto old_val = val.exchange(new_val, tempest::memory_order::relaxed);

    EXPECT_EQ(old_val, 1234567890123456789LL);
    EXPECT_EQ(val.load(), new_val);
}

TEST(atomic_int64_test, exchange_acquire)
{
    auto val = tempest::atomic<tempest::int64_t>{1234567890123456789LL};
    const auto new_val = static_cast<tempest::int64_t>(-2000000000000000000LL);
    const auto old_val = val.exchange(new_val, tempest::memory_order::acquire);

    EXPECT_EQ(old_val, 1234567890123456789LL);
    EXPECT_EQ(val.load(), new_val);
}

TEST(atomic_int64_test, exchange_seq_cst)
{
    auto val = tempest::atomic<tempest::int64_t>{1234567890123456789LL};
    const auto new_val = static_cast<tempest::int64_t>(-2000000000000000000LL);
    const auto old_val = val.exchange(new_val, tempest::memory_order::seq_cst);

    EXPECT_EQ(old_val, 1234567890123456789LL);
    EXPECT_EQ(val.load(), new_val);
}

TEST(atomic_int8_test, compare_exchange_strong)
{
    auto val = tempest::atomic<tempest::int8_t>{42};
    auto expected = static_cast<tempest::int8_t>(42);
    const auto desired = static_cast<tempest::int8_t>(-100);

    const auto result = val.compare_exchange_strong(expected, desired);
    EXPECT_TRUE(result);
    EXPECT_EQ(val.load(), desired);
    EXPECT_EQ(expected, 42); // Expected should remain unchanged on success
}

TEST(atomic_int8_test, compare_exchange_strong_failure)
{
    auto val = tempest::atomic<tempest::int8_t>{42};
    auto expected = static_cast<tempest::int8_t>(100); // Incorrect expected value
    const auto desired = static_cast<tempest::int8_t>(-100);

    const auto result = val.compare_exchange_strong(expected, desired);
    EXPECT_FALSE(result);
    EXPECT_EQ(val.load(), 42); // Value should remain unchanged on failure
    EXPECT_EQ(expected, 42);   // Expected should be updated to the actual value on failure
}

TEST(atomic_int8_test, compare_exchange_strong_relaxed)
{
    auto val = tempest::atomic<tempest::int8_t>{42};
    auto expected = static_cast<tempest::int8_t>(42);
    const auto desired = static_cast<tempest::int8_t>(-100);

    const auto result = val.compare_exchange_strong(expected, desired, tempest::memory_order::relaxed);
    EXPECT_TRUE(result);
    EXPECT_EQ(val.load(), desired);
    EXPECT_EQ(expected, 42); // Expected should remain unchanged on success
}

TEST(atomic_int8_test, compare_exchange_strong_failure_relaxed)
{
    auto val = tempest::atomic<tempest::int8_t>{42};
    auto expected = static_cast<tempest::int8_t>(100); // Incorrect expected value
    const auto desired = static_cast<tempest::int8_t>(-100);

    const auto result = val.compare_exchange_strong(expected, desired, tempest::memory_order::relaxed);
    EXPECT_FALSE(result);
    EXPECT_EQ(val.load(), 42); // Value should remain unchanged on failure
    EXPECT_EQ(expected, 42);   // Expected should be updated to the actual value on failure
}

TEST(atomic_int8_test, compare_exchange_strong_acquire)
{
    auto val = tempest::atomic<tempest::int8_t>{42};
    auto expected = static_cast<tempest::int8_t>(42);
    const auto desired = static_cast<tempest::int8_t>(-100);

    const auto result = val.compare_exchange_strong(expected, desired, tempest::memory_order::acquire);
    EXPECT_TRUE(result);
    EXPECT_EQ(val.load(), desired);
    EXPECT_EQ(expected, 42); // Expected should remain unchanged on success
}

TEST(atomic_int8_test, compare_exchange_strong_failure_acquire)
{
    auto val = tempest::atomic<tempest::int8_t>{42};
    auto expected = static_cast<tempest::int8_t>(100); // Incorrect expected value
    const auto desired = static_cast<tempest::int8_t>(-100);

    const auto result = val.compare_exchange_strong(expected, desired, tempest::memory_order::acquire);
    EXPECT_FALSE(result);
    EXPECT_EQ(val.load(), 42); // Value should remain unchanged on failure
    EXPECT_EQ(expected, 42);   // Expected should be updated to the actual value on failure
}

TEST(atomic_int8_test, compare_exchange_strong_seq_cst)
{
    auto val = tempest::atomic<tempest::int8_t>{42};
    auto expected = static_cast<tempest::int8_t>(42);
    const auto desired = static_cast<tempest::int8_t>(-100);

    const auto result = val.compare_exchange_strong(expected, desired, tempest::memory_order::seq_cst);
    EXPECT_TRUE(result);
    EXPECT_EQ(val.load(), desired);
    EXPECT_EQ(expected, 42); // Expected should remain unchanged on success
}

TEST(atomic_int8_test, compare_exchange_strong_failure_seq_cst)
{
    auto val = tempest::atomic<tempest::int8_t>{42};
    auto expected = static_cast<tempest::int8_t>(100); // Incorrect expected value
    const auto desired = static_cast<tempest::int8_t>(-100);

    const auto result = val.compare_exchange_strong(expected, desired, tempest::memory_order::seq_cst);
    EXPECT_FALSE(result);
    EXPECT_EQ(val.load(), 42); // Value should remain unchanged on failure
    EXPECT_EQ(expected, 42);   // Expected should be updated to the actual value on failure
}

TEST(atomic_int16_test, compare_exchange_strong)
{
    auto val = tempest::atomic<tempest::int16_t>{12345};
    auto expected = static_cast<tempest::int16_t>(12345);
    const auto desired = static_cast<tempest::int16_t>(-20000);

    const auto result = val.compare_exchange_strong(expected, desired);
    EXPECT_TRUE(result);
    EXPECT_EQ(val.load(), desired);
    EXPECT_EQ(expected, 12345); // Expected should remain unchanged on success
}

TEST(atomic_int16_test, compare_exchange_strong_failure)
{
    auto val = tempest::atomic<tempest::int16_t>{12345};
    auto expected = static_cast<tempest::int16_t>(10000); // Incorrect expected value
    const auto desired = static_cast<tempest::int16_t>(-20000);

    const auto result = val.compare_exchange_strong(expected, desired);
    EXPECT_FALSE(result);
    EXPECT_EQ(val.load(), 12345); // Value should remain unchanged on failure
    EXPECT_EQ(expected, 12345);   // Expected should be updated to the actual value on failure
}

TEST(atomic_int16_test, compare_exchange_strong_relaxed)
{
    auto val = tempest::atomic<tempest::int16_t>{12345};
    auto expected = static_cast<tempest::int16_t>(12345);
    const auto desired = static_cast<tempest::int16_t>(-20000);

    const auto result = val.compare_exchange_strong(expected, desired, tempest::memory_order::relaxed);
    EXPECT_TRUE(result);
    EXPECT_EQ(val.load(), desired);
    EXPECT_EQ(expected, 12345); // Expected should remain unchanged on success
}

TEST(atomic_int16_test, compare_exchange_strong_failure_relaxed)
{
    auto val = tempest::atomic<tempest::int16_t>{12345};
    auto expected = static_cast<tempest::int16_t>(10000); // Incorrect expected value
    const auto desired = static_cast<tempest::int16_t>(-20000);

    const auto result = val.compare_exchange_strong(expected, desired, tempest::memory_order::relaxed);
    EXPECT_FALSE(result);
    EXPECT_EQ(val.load(), 12345); // Value should remain unchanged on failure
    EXPECT_EQ(expected, 12345);   // Expected should be updated to the actual value on failure
}

TEST(atomic_int16_test, compare_exchange_strong_acquire)
{
    auto val = tempest::atomic<tempest::int16_t>{12345};
    auto expected = static_cast<tempest::int16_t>(12345);
    const auto desired = static_cast<tempest::int16_t>(-20000);

    const auto result = val.compare_exchange_strong(expected, desired, tempest::memory_order::acquire);
    EXPECT_TRUE(result);
    EXPECT_EQ(val.load(), desired);
    EXPECT_EQ(expected, 12345); // Expected should remain unchanged on success
}

TEST(atomic_int16_test, compare_exchange_strong_failure_acquire)
{
    auto val = tempest::atomic<tempest::int16_t>{12345};
    auto expected = static_cast<tempest::int16_t>(10000); // Incorrect expected value
    const auto desired = static_cast<tempest::int16_t>(-20000);

    const auto result = val.compare_exchange_strong(expected, desired, tempest::memory_order::acquire);
    EXPECT_FALSE(result);
    EXPECT_EQ(val.load(), 12345); // Value should remain unchanged on failure
    EXPECT_EQ(expected, 12345);   // Expected should be updated to the actual value on failure
}

TEST(atomic_int16_test, compare_exchange_strong_seq_cst)
{
    auto val = tempest::atomic<tempest::int16_t>{12345};
    auto expected = static_cast<tempest::int16_t>(12345);
    const auto desired = static_cast<tempest::int16_t>(-20000);

    const auto result = val.compare_exchange_strong(expected, desired, tempest::memory_order::seq_cst);
    EXPECT_TRUE(result);
    EXPECT_EQ(val.load(), desired);
    EXPECT_EQ(expected, 12345); // Expected should remain unchanged on success
}

TEST(atomic_int16_test, compare_exchange_strong_failure_seq_cst)
{
    auto val = tempest::atomic<tempest::int16_t>{12345};
    auto expected = static_cast<tempest::int16_t>(10000); // Incorrect expected value
    const auto desired = static_cast<tempest::int16_t>(-20000);

    const auto result = val.compare_exchange_strong(expected, desired, tempest::memory_order::seq_cst);
    EXPECT_FALSE(result);
    EXPECT_EQ(val.load(), 12345); // Value should remain unchanged on failure
    EXPECT_EQ(expected, 12345);   // Expected should be updated to the actual value on failure
}

TEST(atomic_int32_test, compare_exchange_strong)
{
    auto val = tempest::atomic<tempest::int32_t>{123456789};
    auto expected = static_cast<tempest::int32_t>(123456789);
    const auto desired = static_cast<tempest::int32_t>(-2000000000);

    const auto result = val.compare_exchange_strong(expected, desired);
    EXPECT_TRUE(result);
    EXPECT_EQ(val.load(), desired);
    EXPECT_EQ(expected, 123456789); // Expected should remain unchanged on success
}

TEST(atomic_int32_test, compare_exchange_strong_failure)
{
    auto val = tempest::atomic<tempest::int32_t>{123456789};
    auto expected = static_cast<tempest::int32_t>(1000000000); // Incorrect expected value
    const auto desired = static_cast<tempest::int32_t>(-2000000000);

    const auto result = val.compare_exchange_strong(expected, desired);
    EXPECT_FALSE(result);
    EXPECT_EQ(val.load(), 123456789); // Value should remain unchanged on failure
    EXPECT_EQ(expected, 123456789);   // Expected should be updated to the actual value on failure
}

TEST(atomic_int32_test, compare_exchange_strong_relaxed)
{
    auto val = tempest::atomic<tempest::int32_t>{123456789};
    auto expected = static_cast<tempest::int32_t>(123456789);
    const auto desired = static_cast<tempest::int32_t>(-2000000000);

    const auto result = val.compare_exchange_strong(expected, desired, tempest::memory_order::relaxed);
    EXPECT_TRUE(result);
    EXPECT_EQ(val.load(), desired);
    EXPECT_EQ(expected, 123456789); // Expected should remain unchanged on success
}

TEST(atomic_int32_test, compare_exchange_strong_failure_relaxed)
{
    auto val = tempest::atomic<tempest::int32_t>{123456789};
    auto expected = static_cast<tempest::int32_t>(1000000000); // Incorrect expected value
    const auto desired = static_cast<tempest::int32_t>(-2000000000);

    const auto result = val.compare_exchange_strong(expected, desired, tempest::memory_order::relaxed);
    EXPECT_FALSE(result);
    EXPECT_EQ(val.load(), 123456789); // Value should remain unchanged on failure
    EXPECT_EQ(expected, 123456789);   // Expected should be updated to the actual value on failure
}

TEST(atomic_int32_test, compare_exchange_strong_acquire)
{
    auto val = tempest::atomic<tempest::int32_t>{123456789};
    auto expected = static_cast<tempest::int32_t>(123456789);
    const auto desired = static_cast<tempest::int32_t>(-2000000000);

    const auto result = val.compare_exchange_strong(expected, desired, tempest::memory_order::acquire);
    EXPECT_TRUE(result);
    EXPECT_EQ(val.load(), desired);
    EXPECT_EQ(expected, 123456789); // Expected should remain unchanged on success
}

TEST(atomic_int32_test, compare_exchange_strong_failure_acquire)
{
    auto val = tempest::atomic<tempest::int32_t>{123456789};
    auto expected = static_cast<tempest::int32_t>(1000000000); // Incorrect expected value
    const auto desired = static_cast<tempest::int32_t>(-2000000000);

    const auto result = val.compare_exchange_strong(expected, desired, tempest::memory_order::acquire);
    EXPECT_FALSE(result);
    EXPECT_EQ(val.load(), 123456789); // Value should remain unchanged on failure
    EXPECT_EQ(expected, 123456789);   // Expected should be updated to the actual value on failure
}

TEST(atomic_int32_test, compare_exchange_strong_seq_cst)
{
    auto val = tempest::atomic<tempest::int32_t>{123456789};
    auto expected = static_cast<tempest::int32_t>(123456789);
    const auto desired = static_cast<tempest::int32_t>(-2000000000);

    const auto result = val.compare_exchange_strong(expected, desired, tempest::memory_order::seq_cst);
    EXPECT_TRUE(result);
    EXPECT_EQ(val.load(), desired);
    EXPECT_EQ(expected, 123456789); // Expected should remain unchanged on success
}

TEST(atomic_int32_test, compare_exchange_strong_failure_seq_cst)
{
    auto val = tempest::atomic<tempest::int32_t>{123456789};
    auto expected = static_cast<tempest::int32_t>(1000000000); // Incorrect expected value
    const auto desired = static_cast<tempest::int32_t>(-2000000000);

    const auto result = val.compare_exchange_strong(expected, desired, tempest::memory_order::seq_cst);
    EXPECT_FALSE(result);
    EXPECT_EQ(val.load(), 123456789); // Value should remain unchanged on failure
    EXPECT_EQ(expected, 123456789);   // Expected should be updated to the actual value on failure
}

TEST(atomic_int64_test, compare_exchange_strong)
{
    auto val = tempest::atomic<tempest::int64_t>{1234567890123456789LL};
    auto expected = static_cast<tempest::int64_t>(1234567890123456789LL);
    const auto desired = static_cast<tempest::int64_t>(-2000000000000000000LL);

    const auto result = val.compare_exchange_strong(expected, desired);
    EXPECT_TRUE(result);
    EXPECT_EQ(val.load(), desired);
    EXPECT_EQ(expected, 1234567890123456789LL); // Expected should remain unchanged on success
}

TEST(atomic_int64_test, compare_exchange_strong_failure)
{
    auto val = tempest::atomic<tempest::int64_t>{1234567890123456789LL};
    auto expected = static_cast<tempest::int64_t>(1000000000000000000LL); // Incorrect expected value
    const auto desired = static_cast<tempest::int64_t>(-2000000000000000000LL);

    const auto result = val.compare_exchange_strong(expected, desired);
    EXPECT_FALSE(result);
    EXPECT_EQ(val.load(), 1234567890123456789LL); // Value should remain unchanged on failure
    EXPECT_EQ(expected, 1234567890123456789LL);   // Expected should be updated to the actual value on failure
}

TEST(atomic_int64_test, compare_exchange_strong_relaxed)
{
    auto val = tempest::atomic<tempest::int64_t>{1234567890123456789LL};
    auto expected = static_cast<tempest::int64_t>(1234567890123456789LL);
    const auto desired = static_cast<tempest::int64_t>(-2000000000000000000LL);

    const auto result = val.compare_exchange_strong(expected, desired, tempest::memory_order::relaxed);
    EXPECT_TRUE(result);
    EXPECT_EQ(val.load(), desired);
    EXPECT_EQ(expected, 1234567890123456789LL); // Expected should remain unchanged on success
}

TEST(atomic_int64_test, compare_exchange_strong_failure_relaxed)
{
    auto val = tempest::atomic<tempest::int64_t>{1234567890123456789LL};
    auto expected = static_cast<tempest::int64_t>(1000000000000000000LL); // Incorrect expected value
    const auto desired = static_cast<tempest::int64_t>(-2000000000000000000LL);

    const auto result = val.compare_exchange_strong(expected, desired, tempest::memory_order::relaxed);
    EXPECT_FALSE(result);
    EXPECT_EQ(val.load(), 1234567890123456789LL); // Value should remain unchanged on failure
    EXPECT_EQ(expected, 1234567890123456789LL);   // Expected should be updated to the actual value on failure
}

TEST(atomic_int64_test, compare_exchange_strong_acquire)
{
    auto val = tempest::atomic<tempest::int64_t>{1234567890123456789LL};
    auto expected = static_cast<tempest::int64_t>(1234567890123456789LL);
    const auto desired = static_cast<tempest::int64_t>(-2000000000000000000LL);

    const auto result = val.compare_exchange_strong(expected, desired, tempest::memory_order::acquire);
    EXPECT_TRUE(result);
    EXPECT_EQ(val.load(), desired);
    EXPECT_EQ(expected, 1234567890123456789LL); // Expected should remain unchanged on success
}

TEST(atomic_int64_test, compare_exchange_strong_failure_acquire)
{
    auto val = tempest::atomic<tempest::int64_t>{1234567890123456789LL};
    auto expected = static_cast<tempest::int64_t>(1000000000000000000LL); // Incorrect expected value
    const auto desired = static_cast<tempest::int64_t>(-2000000000000000000LL);

    const auto result = val.compare_exchange_strong(expected, desired, tempest::memory_order::acquire);
    EXPECT_FALSE(result);
    EXPECT_EQ(val.load(), 1234567890123456789LL); // Value should remain unchanged on failure
    EXPECT_EQ(expected, 1234567890123456789LL);   // Expected should be updated to the actual value on failure
}

TEST(atomic_int64_test, compare_exchange_strong_seq_cst)
{
    auto val = tempest::atomic<tempest::int64_t>{1234567890123456789LL};
    auto expected = static_cast<tempest::int64_t>(1234567890123456789LL);
    const auto desired = static_cast<tempest::int64_t>(-2000000000000000000LL);

    const auto result = val.compare_exchange_strong(expected, desired, tempest::memory_order::seq_cst);
    EXPECT_TRUE(result);
    EXPECT_EQ(val.load(), desired);
    EXPECT_EQ(expected, 1234567890123456789LL); // Expected should remain unchanged on success
}

TEST(atomic_int64_test, compare_exchange_strong_failure_seq_cst)
{
    auto val = tempest::atomic<tempest::int64_t>{1234567890123456789LL};
    auto expected = static_cast<tempest::int64_t>(1000000000000000000LL); // Incorrect expected value
    const auto desired = static_cast<tempest::int64_t>(-2000000000000000000LL);

    const auto result = val.compare_exchange_strong(expected, desired, tempest::memory_order::seq_cst);
    EXPECT_FALSE(result);
    EXPECT_EQ(val.load(), 1234567890123456789LL); // Value should remain unchanged on failure
    EXPECT_EQ(expected, 1234567890123456789LL);   // Expected should be updated to the actual value on failure
}

TEST(atomic_int8_test, fetch_add)
{
    auto val = tempest::atomic<tempest::int8_t>{42};
    const auto old_val = val.fetch_add(10);

    EXPECT_EQ(old_val, 42);
    EXPECT_EQ(val.load(), 52);
}

TEST(atomic_int8_test, fetch_add_relaxed)
{
    auto val = tempest::atomic<tempest::int8_t>{42};
    const auto old_val = val.fetch_add(10, tempest::memory_order::relaxed);

    EXPECT_EQ(old_val, 42);
    EXPECT_EQ(val.load(), 52);
}

TEST(atomic_int8_test, fetch_add_acquire)
{
    auto val = tempest::atomic<tempest::int8_t>{42};
    const auto old_val = val.fetch_add(10, tempest::memory_order::acquire);

    EXPECT_EQ(old_val, 42);
    EXPECT_EQ(val.load(), 52);
}

TEST(atomic_int8_test, fetch_add_acq_rel)
{
    auto val = tempest::atomic<tempest::int8_t>{42};
    const auto old_val = val.fetch_add(10, tempest::memory_order::acq_rel);

    EXPECT_EQ(old_val, 42);
    EXPECT_EQ(val.load(), 52);
}

TEST(atomic_int8_test, fetch_add_seq_cst)
{
    auto val = tempest::atomic<tempest::int8_t>{42};
    const auto old_val = val.fetch_add(10, tempest::memory_order::seq_cst);

    EXPECT_EQ(old_val, 42);
    EXPECT_EQ(val.load(), 52);
}

TEST(atomic_int16_test, fetch_add)
{
    auto val = tempest::atomic<tempest::int16_t>{12345};
    const auto old_val = val.fetch_add(1000);

    EXPECT_EQ(old_val, 12345);
    EXPECT_EQ(val.load(), 13345);
}

TEST(atomic_int16_test, fetch_add_relaxed)
{
    auto val = tempest::atomic<tempest::int16_t>{12345};
    const auto old_val = val.fetch_add(1000, tempest::memory_order::relaxed);

    EXPECT_EQ(old_val, 12345);
    EXPECT_EQ(val.load(), 13345);
}

TEST(atomic_int16_test, fetch_add_acquire)
{
    auto val = tempest::atomic<tempest::int16_t>{12345};
    const auto old_val = val.fetch_add(1000, tempest::memory_order::acquire);

    EXPECT_EQ(old_val, 12345);
    EXPECT_EQ(val.load(), 13345);
}

TEST(atomic_int16_test, fetch_add_acq_rel)
{
    auto val = tempest::atomic<tempest::int16_t>{12345};
    const auto old_val = val.fetch_add(1000, tempest::memory_order::acq_rel);

    EXPECT_EQ(old_val, 12345);
    EXPECT_EQ(val.load(), 13345);
}

TEST(atomic_int16_test, fetch_add_seq_cst)
{
    auto val = tempest::atomic<tempest::int16_t>{12345};
    const auto old_val = val.fetch_add(1000, tempest::memory_order::seq_cst);

    EXPECT_EQ(old_val, 12345);
    EXPECT_EQ(val.load(), 13345);
}

TEST(atomic_int32_test, fetch_add)
{
    auto val = tempest::atomic<tempest::int32_t>{123456789};
    const auto old_val = val.fetch_add(10000000);

    EXPECT_EQ(old_val, 123456789);
    EXPECT_EQ(val.load(), 133456789);
}

TEST(atomic_int32_test, fetch_add_relaxed)
{
    auto val = tempest::atomic<tempest::int32_t>{123456789};
    const auto old_val = val.fetch_add(10000000, tempest::memory_order::relaxed);

    EXPECT_EQ(old_val, 123456789);
    EXPECT_EQ(val.load(), 133456789);
}

TEST(atomic_int32_test, fetch_add_acquire)
{
    auto val = tempest::atomic<tempest::int32_t>{123456789};
    const auto old_val = val.fetch_add(10000000, tempest::memory_order::acquire);

    EXPECT_EQ(old_val, 123456789);
    EXPECT_EQ(val.load(), 133456789);
}

TEST(atomic_int32_test, fetch_add_acq_rel)
{
    auto val = tempest::atomic<tempest::int32_t>{123456789};
    const auto old_val = val.fetch_add(10000000, tempest::memory_order::acq_rel);

    EXPECT_EQ(old_val, 123456789);
    EXPECT_EQ(val.load(), 133456789);
}

TEST(atomic_int32_test, fetch_add_seq_cst)
{
    auto val = tempest::atomic<tempest::int32_t>{123456789};
    const auto old_val = val.fetch_add(10000000, tempest::memory_order::seq_cst);

    EXPECT_EQ(old_val, 123456789);
    EXPECT_EQ(val.load(), 133456789);
}

TEST(atomic_int64_test, fetch_add)
{
    auto val = tempest::atomic<tempest::int64_t>{1234567890123456789LL};
    const auto old_val = val.fetch_add(100000000000000000LL);

    EXPECT_EQ(old_val, 1234567890123456789LL);
    EXPECT_EQ(val.load(), 1334567890123456789LL);
}

TEST(atomic_int64_test, fetch_add_relaxed)
{
    auto val = tempest::atomic<tempest::int64_t>{1234567890123456789LL};
    const auto old_val = val.fetch_add(100000000000000000LL, tempest::memory_order::relaxed);

    EXPECT_EQ(old_val, 1234567890123456789LL);
    EXPECT_EQ(val.load(), 1334567890123456789LL);
}

TEST(atomic_int64_test, fetch_add_acquire)
{
    auto val = tempest::atomic<tempest::int64_t>{1234567890123456789LL};
    const auto old_val = val.fetch_add(100000000000000000LL, tempest::memory_order::acquire);

    EXPECT_EQ(old_val, 1234567890123456789LL);
    EXPECT_EQ(val.load(), 1334567890123456789LL);
}

TEST(atomic_int64_test, fetch_add_acq_rel)
{
    auto val = tempest::atomic<tempest::int64_t>{1234567890123456789LL};
    const auto old_val = val.fetch_add(100000000000000000LL, tempest::memory_order::acq_rel);

    EXPECT_EQ(old_val, 1234567890123456789LL);
    EXPECT_EQ(val.load(), 1334567890123456789LL);
}

TEST(atomic_int64_test, fetch_add_seq_cst)
{
    auto val = tempest::atomic<tempest::int64_t>{1234567890123456789LL};
    const auto old_val = val.fetch_add(100000000000000000LL, tempest::memory_order::seq_cst);

    EXPECT_EQ(old_val, 1234567890123456789LL);
    EXPECT_EQ(val.load(), 1334567890123456789LL);
}

TEST(atomic_int8_test, fetch_sub)
{
    auto val = tempest::atomic<tempest::int8_t>{42};
    const auto old_val = val.fetch_sub(10);

    EXPECT_EQ(old_val, 42);
    EXPECT_EQ(val.load(), 32);
}

TEST(atomic_int8_test, fetch_sub_relaxed)
{
    auto val = tempest::atomic<tempest::int8_t>{42};
    const auto old_val = val.fetch_sub(10, tempest::memory_order::relaxed);

    EXPECT_EQ(old_val, 42);
    EXPECT_EQ(val.load(), 32);
}

TEST(atomic_int8_test, fetch_sub_acquire)
{
    auto val = tempest::atomic<tempest::int8_t>{42};
    const auto old_val = val.fetch_sub(10, tempest::memory_order::acquire);

    EXPECT_EQ(old_val, 42);
    EXPECT_EQ(val.load(), 32);
}

TEST(atomic_int8_test, fetch_sub_acq_rel)
{
    auto val = tempest::atomic<tempest::int8_t>{42};
    const auto old_val = val.fetch_sub(10, tempest::memory_order::acq_rel);

    EXPECT_EQ(old_val, 42);
    EXPECT_EQ(val.load(), 32);
}

TEST(atomic_int8_test, fetch_sub_seq_cst)
{
    auto val = tempest::atomic<tempest::int8_t>{42};
    const auto old_val = val.fetch_sub(10, tempest::memory_order::seq_cst);

    EXPECT_EQ(old_val, 42);
    EXPECT_EQ(val.load(), 32);
}

TEST(atomic_int16_test, fetch_sub)
{
    auto val = tempest::atomic<tempest::int16_t>{12345};
    const auto old_val = val.fetch_sub(1000);

    EXPECT_EQ(old_val, 12345);
    EXPECT_EQ(val.load(), 11345);
}

TEST(atomic_int16_test, fetch_sub_relaxed)
{
    auto val = tempest::atomic<tempest::int16_t>{12345};
    const auto old_val = val.fetch_sub(1000, tempest::memory_order::relaxed);

    EXPECT_EQ(old_val, 12345);
    EXPECT_EQ(val.load(), 11345);
}

TEST(atomic_int16_test, fetch_sub_acquire)
{
    auto val = tempest::atomic<tempest::int16_t>{12345};
    const auto old_val = val.fetch_sub(1000, tempest::memory_order::acquire);

    EXPECT_EQ(old_val, 12345);
    EXPECT_EQ(val.load(), 11345);
}

TEST(atomic_int16_test, fetch_sub_acq_rel)
{
    auto val = tempest::atomic<tempest::int16_t>{12345};
    const auto old_val = val.fetch_sub(1000, tempest::memory_order::acq_rel);

    EXPECT_EQ(old_val, 12345);
    EXPECT_EQ(val.load(), 11345);
}

TEST(atomic_int16_test, fetch_sub_seq_cst)
{
    auto val = tempest::atomic<tempest::int16_t>{12345};
    const auto old_val = val.fetch_sub(1000, tempest::memory_order::seq_cst);

    EXPECT_EQ(old_val, 12345);
    EXPECT_EQ(val.load(), 11345);
}

TEST(atomic_int32_test, fetch_sub)
{
    auto val = tempest::atomic<tempest::int32_t>{123456789};
    const auto old_val = val.fetch_sub(10000000);

    EXPECT_EQ(old_val, 123456789);
    EXPECT_EQ(val.load(), 113456789);
}

TEST(atomic_int32_test, fetch_sub_relaxed)
{
    auto val = tempest::atomic<tempest::int32_t>{123456789};
    const auto old_val = val.fetch_sub(10000000, tempest::memory_order::relaxed);

    EXPECT_EQ(old_val, 123456789);
    EXPECT_EQ(val.load(), 113456789);
}

TEST(atomic_int32_test, fetch_sub_acquire)
{
    auto val = tempest::atomic<tempest::int32_t>{123456789};
    const auto old_val = val.fetch_sub(10000000, tempest::memory_order::acquire);

    EXPECT_EQ(old_val, 123456789);
    EXPECT_EQ(val.load(), 113456789);
}

TEST(atomic_int32_test, fetch_sub_acq_rel)
{
    auto val = tempest::atomic<tempest::int32_t>{123456789};
    const auto old_val = val.fetch_sub(10000000, tempest::memory_order::acq_rel);

    EXPECT_EQ(old_val, 123456789);
    EXPECT_EQ(val.load(), 113456789);
}

TEST(atomic_int32_test, fetch_sub_seq_cst)
{
    auto val = tempest::atomic<tempest::int32_t>{123456789};
    const auto old_val = val.fetch_sub(10000000, tempest::memory_order::seq_cst);

    EXPECT_EQ(old_val, 123456789);
    EXPECT_EQ(val.load(), 113456789);
}

TEST(atomic_int64_test, fetch_sub)
{
    auto val = tempest::atomic<tempest::int64_t>{1234567890123456789LL};
    const auto old_val = val.fetch_sub(100000000000000000LL);

    EXPECT_EQ(old_val, 1234567890123456789LL);
    EXPECT_EQ(val.load(), 1134567890123456789LL);
}

TEST(atomic_int64_test, fetch_sub_relaxed)
{
    auto val = tempest::atomic<tempest::int64_t>{1234567890123456789LL};
    const auto old_val = val.fetch_sub(100000000000000000LL, tempest::memory_order::relaxed);

    EXPECT_EQ(old_val, 1234567890123456789LL);
    EXPECT_EQ(val.load(), 1134567890123456789LL);
}

TEST(atomic_int64_test, fetch_sub_acquire)
{
    auto val = tempest::atomic<tempest::int64_t>{1234567890123456789LL};
    const auto old_val = val.fetch_sub(100000000000000000LL, tempest::memory_order::acquire);

    EXPECT_EQ(old_val, 1234567890123456789LL);
    EXPECT_EQ(val.load(), 1134567890123456789LL);
}

TEST(atomic_int64_test, fetch_sub_acq_rel)
{
    auto val = tempest::atomic<tempest::int64_t>{1234567890123456789LL};
    const auto old_val = val.fetch_sub(100000000000000000LL, tempest::memory_order::acq_rel);

    EXPECT_EQ(old_val, 1234567890123456789LL);
    EXPECT_EQ(val.load(), 1134567890123456789LL);
}

TEST(atomic_int64_test, fetch_sub_seq_cst)
{
    auto val = tempest::atomic<tempest::int64_t>{1234567890123456789LL};
    const auto old_val = val.fetch_sub(100000000000000000LL, tempest::memory_order::seq_cst);

    EXPECT_EQ(old_val, 1234567890123456789LL);
    EXPECT_EQ(val.load(), 1134567890123456789LL);
}

TEST(atomic_int8_test, fetch_and)
{
    auto val = tempest::atomic<tempest::int8_t>{static_cast<signed char>(0b10101010)};
    const auto old_val = val.fetch_and(static_cast<signed char>(0b11001100));

    EXPECT_EQ(old_val, static_cast<signed char>(0b10101010));
    EXPECT_EQ(val.load(), static_cast<signed char>(0b10001000));
}

TEST(atomic_int8_test, fetch_and_relaxed)
{
    auto val = tempest::atomic<tempest::int8_t>{static_cast<signed char>(0b10101010)};
    const auto old_val = val.fetch_and(static_cast<signed char>(0b11001100), tempest::memory_order::relaxed);

    EXPECT_EQ(old_val, static_cast<signed char>(0b10101010));
    EXPECT_EQ(val.load(), static_cast<signed char>(0b10001000));
}

TEST(atomic_int8_test, fetch_and_acquire)
{
    auto val = tempest::atomic<tempest::int8_t>{static_cast<signed char>(0b10101010)};
    const auto old_val = val.fetch_and(static_cast<signed char>(0b11001100), tempest::memory_order::acquire);

    EXPECT_EQ(old_val, static_cast<signed char>(0b10101010));
    EXPECT_EQ(val.load(), static_cast<signed char>(0b10001000));
}

TEST(atomic_int8_test, fetch_and_acq_rel)
{
    auto val = tempest::atomic<tempest::int8_t>{static_cast<signed char>(0b10101010)};
    const auto old_val = val.fetch_and(static_cast<signed char>(0b11001100), tempest::memory_order::acq_rel);

    EXPECT_EQ(old_val, static_cast<signed char>(0b10101010));
    EXPECT_EQ(val.load(), static_cast<signed char>(0b10001000));
}

TEST(atomic_int8_test, fetch_and_seq_cst)
{
    auto val = tempest::atomic<tempest::int8_t>{static_cast<signed char>(0b10101010)};
    const auto old_val = val.fetch_and(static_cast<signed char>(0b11001100), tempest::memory_order::seq_cst);

    EXPECT_EQ(old_val, static_cast<signed char>(0b10101010));
    EXPECT_EQ(val.load(), static_cast<signed char>(0b10001000));
}

TEST(atomic_int16_test, fetch_and)
{
    auto val = tempest::atomic<tempest::int16_t>{static_cast<short>(0b1010101010101010)};
    const auto old_val = val.fetch_and(static_cast<short>(0b1100110011001100));

    EXPECT_EQ(old_val, static_cast<short>(0b1010101010101010));
    EXPECT_EQ(val.load(), static_cast<short>(0b1000100010001000));
}

TEST(atomic_int16_test, fetch_and_relaxed)
{
    auto val = tempest::atomic<tempest::int16_t>{static_cast<short>(0b1010101010101010)};
    const auto old_val = val.fetch_and(static_cast<short>(0b1100110011001100), tempest::memory_order::relaxed);

    EXPECT_EQ(old_val, static_cast<short>(0b1010101010101010));
    EXPECT_EQ(val.load(), static_cast<short>(0b1000100010001000));
}

TEST(atomic_int16_test, fetch_and_acquire)
{
    auto val = tempest::atomic<tempest::int16_t>{static_cast<short>(0b1010101010101010)};
    const auto old_val = val.fetch_and(static_cast<short>(0b1100110011001100), tempest::memory_order::acquire);

    EXPECT_EQ(old_val, static_cast<short>(0b1010101010101010));
    EXPECT_EQ(val.load(), static_cast<short>(0b1000100010001000));
}

TEST(atomic_int16_test, fetch_and_acq_rel)
{
    auto val = tempest::atomic<tempest::int16_t>{static_cast<short>(0b1010101010101010)};
    const auto old_val = val.fetch_and(static_cast<short>(0b1100110011001100), tempest::memory_order::acq_rel);

    EXPECT_EQ(old_val, static_cast<short>(0b1010101010101010));
    EXPECT_EQ(val.load(), static_cast<short>(0b1000100010001000));
}

TEST(atomic_int16_test, fetch_and_seq_cst)
{
    auto val = tempest::atomic<tempest::int16_t>{static_cast<short>(0b1010101010101010)};
    const auto old_val = val.fetch_and(static_cast<short>(0b1100110011001100), tempest::memory_order::seq_cst);

    EXPECT_EQ(old_val, static_cast<short>(0b1010101010101010));
    EXPECT_EQ(val.load(), static_cast<short>(0b1000100010001000));
}

TEST(atomic_int32_test, fetch_and)
{
    auto val = tempest::atomic<tempest::int32_t>{static_cast<int>(0b10101010101010101010101010101010)};
    const auto old_val = val.fetch_and(static_cast<int>(0b11001100110011001100110011001100));

    EXPECT_EQ(old_val, static_cast<int>(0b10101010101010101010101010101010));
    EXPECT_EQ(val.load(), static_cast<int>(0b10001000100010001000100010001000));
}

TEST(atomic_int32_test, fetch_and_relaxed)
{
    auto val = tempest::atomic<tempest::int32_t>{static_cast<int>(0b10101010101010101010101010101010)};
    const auto old_val =
        val.fetch_and(static_cast<int>(0b11001100110011001100110011001100), tempest::memory_order::relaxed);

    EXPECT_EQ(old_val, static_cast<int>(0b10101010101010101010101010101010));
    EXPECT_EQ(val.load(), static_cast<int>(0b10001000100010001000100010001000));
}

TEST(atomic_int32_test, fetch_and_acquire)
{
    auto val = tempest::atomic<tempest::int32_t>{static_cast<int>(0b10101010101010101010101010101010)};
    const auto old_val =
        val.fetch_and(static_cast<int>(0b11001100110011001100110011001100), tempest::memory_order::acquire);

    EXPECT_EQ(old_val, static_cast<int>(0b10101010101010101010101010101010));
    EXPECT_EQ(val.load(), static_cast<int>(0b10001000100010001000100010001000));
}

TEST(atomic_int32_test, fetch_and_acq_rel)
{
    auto val = tempest::atomic<tempest::int32_t>{static_cast<int>(0b10101010101010101010101010101010)};
    const auto old_val =
        val.fetch_and(static_cast<int>(0b11001100110011001100110011001100), tempest::memory_order::acq_rel);

    EXPECT_EQ(old_val, static_cast<int>(0b10101010101010101010101010101010));
    EXPECT_EQ(val.load(), static_cast<int>(0b10001000100010001000100010001000));
}

TEST(atomic_int32_test, fetch_and_seq_cst)
{
    auto val = tempest::atomic<tempest::int32_t>{static_cast<int>(0b10101010101010101010101010101010)};
    const auto old_val =
        val.fetch_and(static_cast<int>(0b11001100110011001100110011001100), tempest::memory_order::seq_cst);

    EXPECT_EQ(old_val, static_cast<int>(0b10101010101010101010101010101010));
    EXPECT_EQ(val.load(), static_cast<int>(0b10001000100010001000100010001000));
}

TEST(atomic_int64_test, fetch_and)
{
    auto val = tempest::atomic<tempest::int64_t>{
        static_cast<long long>(0b1010101010101010101010101010101010101010101010101010101010101010LL)};
    const auto old_val =
        val.fetch_and(static_cast<long long>(0b1100110011001100110011001100110011001100110011001100110011001100LL));

    EXPECT_EQ(old_val, static_cast<long long>(0b1010101010101010101010101010101010101010101010101010101010101010LL));
    EXPECT_EQ(val.load(), static_cast<long long>(0b1000100010001000100010001000100010001000100010001000100010001000LL));
}

TEST(atomic_int64_test, fetch_and_relaxed)
{
    auto val = tempest::atomic<tempest::int64_t>{
        static_cast<long long>(0b1010101010101010101010101010101010101010101010101010101010101010LL)};
    const auto old_val =
        val.fetch_and(static_cast<long long>(0b1100110011001100110011001100110011001100110011001100110011001100LL),
                      tempest::memory_order::relaxed);

    EXPECT_EQ(old_val, static_cast<long long>(0b1010101010101010101010101010101010101010101010101010101010101010LL));
    EXPECT_EQ(val.load(), static_cast<long long>(0b1000100010001000100010001000100010001000100010001000100010001000LL));
}

TEST(atomic_int64_test, fetch_and_acquire)
{
    auto val = tempest::atomic<tempest::int64_t>{
        static_cast<long long>(0b1010101010101010101010101010101010101010101010101010101010101010LL)};
    const auto old_val =
        val.fetch_and(static_cast<long long>(0b1100110011001100110011001100110011001100110011001100110011001100LL),
                      tempest::memory_order::acquire);

    EXPECT_EQ(old_val, static_cast<long long>(0b1010101010101010101010101010101010101010101010101010101010101010LL));
    EXPECT_EQ(val.load(), static_cast<long long>(0b1000100010001000100010001000100010001000100010001000100010001000LL));
}

TEST(atomic_int64_test, fetch_and_acq_rel)
{
    auto val = tempest::atomic<tempest::int64_t>{
        static_cast<long long>(0b1010101010101010101010101010101010101010101010101010101010101010LL)};
    const auto old_val =
        val.fetch_and(static_cast<long long>(0b1100110011001100110011001100110011001100110011001100110011001100LL),
                      tempest::memory_order::acq_rel);

    EXPECT_EQ(old_val, static_cast<long long>(0b1010101010101010101010101010101010101010101010101010101010101010LL));
    EXPECT_EQ(val.load(), static_cast<long long>(0b1000100010001000100010001000100010001000100010001000100010001000LL));
}

TEST(atomic_int64_test, fetch_and_seq_cst)
{
    auto val = tempest::atomic<tempest::int64_t>{
        static_cast<long long>(0b1010101010101010101010101010101010101010101010101010101010101010LL)};
    const auto old_val =
        val.fetch_and(static_cast<long long>(0b1100110011001100110011001100110011001100110011001100110011001100LL),
                      tempest::memory_order::seq_cst);

    EXPECT_EQ(old_val, static_cast<long long>(0b1010101010101010101010101010101010101010101010101010101010101010LL));
    EXPECT_EQ(val.load(), static_cast<long long>(0b1000100010001000100010001000100010001000100010001000100010001000LL));
}

TEST(atomic_int8_test, fetch_or)
{
    auto val = tempest::atomic<tempest::int8_t>{static_cast<signed char>(0b10101010)};
    const auto old_val = val.fetch_or(static_cast<signed char>(0b11001100));

    EXPECT_EQ(old_val, static_cast<signed char>(0b10101010));
    EXPECT_EQ(val.load(), static_cast<signed char>(0b11101110));
}

TEST(atomic_int8_test, fetch_or_relaxed)
{
    auto val = tempest::atomic<tempest::int8_t>{static_cast<signed char>(0b10101010)};
    const auto old_val = val.fetch_or(static_cast<signed char>(0b11001100), tempest::memory_order::relaxed);

    EXPECT_EQ(old_val, static_cast<signed char>(0b10101010));
    EXPECT_EQ(val.load(), static_cast<signed char>(0b11101110));
}

TEST(atomic_int8_test, fetch_or_acquire)
{
    auto val = tempest::atomic<tempest::int8_t>{static_cast<signed char>(0b10101010)};
    const auto old_val = val.fetch_or(static_cast<signed char>(0b11001100), tempest::memory_order::acquire);

    EXPECT_EQ(old_val, static_cast<signed char>(0b10101010));
    EXPECT_EQ(val.load(), static_cast<signed char>(0b11101110));
}

TEST(atomic_int8_test, fetch_or_acq_rel)
{
    auto val = tempest::atomic<tempest::int8_t>{static_cast<signed char>(0b10101010)};
    const auto old_val = val.fetch_or(static_cast<signed char>(0b11001100), tempest::memory_order::acq_rel);

    EXPECT_EQ(old_val, static_cast<signed char>(0b10101010));
    EXPECT_EQ(val.load(), static_cast<signed char>(0b11101110));
}

TEST(atomic_int8_test, fetch_or_seq_cst)
{
    auto val = tempest::atomic<tempest::int8_t>{static_cast<signed char>(0b10101010)};
    const auto old_val = val.fetch_or(static_cast<signed char>(0b11001100), tempest::memory_order::seq_cst);

    EXPECT_EQ(old_val, static_cast<signed char>(0b10101010));
    EXPECT_EQ(val.load(), static_cast<signed char>(0b11101110));
}

TEST(atomic_int16_test, fetch_or)
{
    auto val = tempest::atomic<tempest::int16_t>{static_cast<short>(0b1010101010101010)};
    const auto old_val = val.fetch_or(static_cast<short>(0b1100110011001100));

    EXPECT_EQ(old_val, static_cast<short>(0b1010101010101010));
    EXPECT_EQ(val.load(), static_cast<short>(0b1110111011101110));
}

TEST(atomic_int16_test, fetch_or_relaxed)
{
    auto val = tempest::atomic<tempest::int16_t>{static_cast<short>(0b1010101010101010)};
    const auto old_val = val.fetch_or(static_cast<short>(0b1100110011001100), tempest::memory_order::relaxed);

    EXPECT_EQ(old_val, static_cast<short>(0b1010101010101010));
    EXPECT_EQ(val.load(), static_cast<short>(0b1110111011101110));
}

TEST(atomic_int16_test, fetch_or_acquire)
{
    auto val = tempest::atomic<tempest::int16_t>{static_cast<short>(0b1010101010101010)};
    const auto old_val = val.fetch_or(static_cast<short>(0b1100110011001100), tempest::memory_order::acquire);

    EXPECT_EQ(old_val, static_cast<short>(0b1010101010101010));
    EXPECT_EQ(val.load(), static_cast<short>(0b1110111011101110));
}

TEST(atomic_int16_test, fetch_or_acq_rel)
{
    auto val = tempest::atomic<tempest::int16_t>{static_cast<short>(0b1010101010101010)};
    const auto old_val = val.fetch_or(static_cast<short>(0b1100110011001100), tempest::memory_order::acq_rel);

    EXPECT_EQ(old_val, static_cast<short>(0b1010101010101010));
    EXPECT_EQ(val.load(), static_cast<short>(0b1110111011101110));
}

TEST(atomic_int16_test, fetch_or_seq_cst)
{
    auto val = tempest::atomic<tempest::int16_t>{static_cast<short>(0b1010101010101010)};
    const auto old_val = val.fetch_or(static_cast<short>(0b1100110011001100), tempest::memory_order::seq_cst);

    EXPECT_EQ(old_val, static_cast<short>(0b1010101010101010));
    EXPECT_EQ(val.load(), static_cast<short>(0b1110111011101110));
}

TEST(atomic_int32_test, fetch_or)
{
    auto val = tempest::atomic<tempest::int32_t>{static_cast<int>(0b10101010101010101010101010101010)};
    const auto old_val = val.fetch_or(static_cast<int>(0b11001100110011001100110011001100));

    EXPECT_EQ(old_val, static_cast<int>(0b10101010101010101010101010101010));
    EXPECT_EQ(val.load(), static_cast<int>(0b11101110111011101110111011101110));
}

TEST(atomic_int32_test, fetch_or_relaxed)
{
    auto val = tempest::atomic<tempest::int32_t>{static_cast<int>(0b10101010101010101010101010101010)};
    const auto old_val =
        val.fetch_or(static_cast<int>(0b11001100110011001100110011001100), tempest::memory_order::relaxed);

    EXPECT_EQ(old_val, static_cast<int>(0b10101010101010101010101010101010));
    EXPECT_EQ(val.load(), static_cast<int>(0b11101110111011101110111011101110));
}

TEST(atomic_int32_test, fetch_or_acquire)
{
    auto val = tempest::atomic<tempest::int32_t>{static_cast<int>(0b10101010101010101010101010101010)};
    const auto old_val =
        val.fetch_or(static_cast<int>(0b11001100110011001100110011001100), tempest::memory_order::acquire);

    EXPECT_EQ(old_val, static_cast<int>(0b10101010101010101010101010101010));
    EXPECT_EQ(val.load(), static_cast<int>(0b11101110111011101110111011101110));
}

TEST(atomic_int32_test, fetch_or_acq_rel)
{
    auto val = tempest::atomic<tempest::int32_t>{static_cast<int>(0b10101010101010101010101010101010)};
    const auto old_val =
        val.fetch_or(static_cast<int>(0b11001100110011001100110011001100), tempest::memory_order::acq_rel);

    EXPECT_EQ(old_val, static_cast<int>(0b10101010101010101010101010101010));
    EXPECT_EQ(val.load(), static_cast<int>(0b11101110111011101110111011101110));
}

TEST(atomic_int32_test, fetch_or_seq_cst)
{
    auto val = tempest::atomic<tempest::int32_t>{static_cast<int>(0b10101010101010101010101010101010)};
    const auto old_val =
        val.fetch_or(static_cast<int>(0b11001100110011001100110011001100), tempest::memory_order::seq_cst);

    EXPECT_EQ(old_val, static_cast<int>(0b10101010101010101010101010101010));
    EXPECT_EQ(val.load(), static_cast<int>(0b11101110111011101110111011101110));
}

TEST(atomic_int64_test, fetch_or)
{
    auto val = tempest::atomic<tempest::int64_t>{
        static_cast<long long>(0b1010101010101010101010101010101010101010101010101010101010101010LL)};
    const auto old_val =
        val.fetch_or(static_cast<long long>(0b1100110011001100110011001100110011001100110011001100110011001100LL));

    EXPECT_EQ(old_val, static_cast<long long>(0b1010101010101010101010101010101010101010101010101010101010101010LL));
    EXPECT_EQ(val.load(), static_cast<long long>(0b1110111011101110111011101110111011101110111011101110111011101110LL));
}

TEST(atomic_int64_test, fetch_or_relaxed)
{
    auto val = tempest::atomic<tempest::int64_t>{
        static_cast<long long>(0b1010101010101010101010101010101010101010101010101010101010101010LL)};
    const auto old_val =
        val.fetch_or(static_cast<long long>(0b1100110011001100110011001100110011001100110011001100110011001100LL),
                      tempest::memory_order::relaxed);

    EXPECT_EQ(old_val, static_cast<long long>(0b1010101010101010101010101010101010101010101010101010101010101010LL));
    EXPECT_EQ(val.load(), static_cast<long long>(0b1110111011101110111011101110111011101110111011101110111011101110LL));
}

TEST(atomic_int64_test, fetch_or_acquire)
{
    auto val = tempest::atomic<tempest::int64_t>{
        static_cast<long long>(0b1010101010101010101010101010101010101010101010101010101010101010LL)};
    const auto old_val =
        val.fetch_or(static_cast<long long>(0b1100110011001100110011001100110011001100110011001100110011001100LL),
                      tempest::memory_order::acquire);

    EXPECT_EQ(old_val, static_cast<long long>(0b1010101010101010101010101010101010101010101010101010101010101010LL));
    EXPECT_EQ(val.load(), static_cast<long long>(0b1110111011101110111011101110111011101110111011101110111011101110LL));
}

TEST(atomic_int64_test, fetch_or_acq_rel)
{
    auto val = tempest::atomic<tempest::int64_t>{
        static_cast<long long>(0b1010101010101010101010101010101010101010101010101010101010101010LL)};
    const auto old_val =
        val.fetch_or(static_cast<long long>(0b1100110011001100110011001100110011001100110011001100110011001100LL),
                      tempest::memory_order::acq_rel);

    EXPECT_EQ(old_val, static_cast<long long>(0b1010101010101010101010101010101010101010101010101010101010101010LL));
    EXPECT_EQ(val.load(), static_cast<long long>(0b1110111011101110111011101110111011101110111011101110111011101110LL));
}

TEST(atomic_int64_test, fetch_or_seq_cst)
{
    auto val = tempest::atomic<tempest::int64_t>{
        static_cast<long long>(0b1010101010101010101010101010101010101010101010101010101010101010LL)};
    const auto old_val =
        val.fetch_or(static_cast<long long>(0b1100110011001100110011001100110011001100110011001100110011001100LL),
                      tempest::memory_order::seq_cst);

    EXPECT_EQ(old_val, static_cast<long long>(0b1010101010101010101010101010101010101010101010101010101010101010LL));
    EXPECT_EQ(val.load(), static_cast<long long>(0b1110111011101110111011101110111011101110111011101110111011101110LL));
}

TEST(atomic_int8_test, fetch_xor)
{
    auto val = tempest::atomic<tempest::int8_t>{static_cast<signed char>(0b10101010)};
    const auto old_val = val.fetch_xor(static_cast<signed char>(0b11001100));

    EXPECT_EQ(old_val, static_cast<signed char>(0b10101010));
    EXPECT_EQ(val.load(), static_cast<signed char>(0b01100110));
}

TEST(atomic_int8_test, fetch_xor_relaxed)
{
    auto val = tempest::atomic<tempest::int8_t>{static_cast<signed char>(0b10101010)};
    const auto old_val = val.fetch_xor(static_cast<signed char>(0b11001100), tempest::memory_order::relaxed);

    EXPECT_EQ(old_val, static_cast<signed char>(0b10101010));
    EXPECT_EQ(val.load(), static_cast<signed char>(0b01100110));
}

TEST(atomic_int8_test, fetch_xor_acquire)
{
    auto val = tempest::atomic<tempest::int8_t>{static_cast<signed char>(0b10101010)};
    const auto old_val = val.fetch_xor(static_cast<signed char>(0b11001100), tempest::memory_order::acquire);

    EXPECT_EQ(old_val, static_cast<signed char>(0b10101010));
    EXPECT_EQ(val.load(), static_cast<signed char>(0b01100110));
}

TEST(atomic_int8_test, fetch_xor_acq_rel)
{
    auto val = tempest::atomic<tempest::int8_t>{static_cast<signed char>(0b10101010)};
    const auto old_val = val.fetch_xor(static_cast<signed char>(0b11001100), tempest::memory_order::acq_rel);

    EXPECT_EQ(old_val, static_cast<signed char>(0b10101010));
    EXPECT_EQ(val.load(), static_cast<signed char>(0b01100110));
}

TEST(atomic_int8_test, fetch_xor_seq_cst)
{
    auto val = tempest::atomic<tempest::int8_t>{static_cast<signed char>(0b10101010)};
    const auto old_val = val.fetch_xor(static_cast<signed char>(0b11001100), tempest::memory_order::seq_cst);

    EXPECT_EQ(old_val, static_cast<signed char>(0b10101010));
    EXPECT_EQ(val.load(), static_cast<signed char>(0b01100110));
}

TEST(atomic_int16_test, fetch_xor)
{
    auto val = tempest::atomic<tempest::int16_t>{static_cast<short>(0b1010101010101010)};
    const auto old_val = val.fetch_xor(static_cast<short>(0b1100110011001100));

    EXPECT_EQ(old_val, static_cast<short>(0b1010101010101010));
    EXPECT_EQ(val.load(), static_cast<short>(0b0110011001100110));
}

TEST(atomic_int16_test, fetch_xor_relaxed)
{
    auto val = tempest::atomic<tempest::int16_t>{static_cast<short>(0b1010101010101010)};
    const auto old_val = val.fetch_xor(static_cast<short>(0b1100110011001100), tempest::memory_order::relaxed);

    EXPECT_EQ(old_val, static_cast<short>(0b1010101010101010));
    EXPECT_EQ(val.load(), static_cast<short>(0b0110011001100110));
}

TEST(atomic_int16_test, fetch_xor_acquire)
{
    auto val = tempest::atomic<tempest::int16_t>{static_cast<short>(0b1010101010101010)};
    const auto old_val = val.fetch_xor(static_cast<short>(0b1100110011001100), tempest::memory_order::acquire);

    EXPECT_EQ(old_val, static_cast<short>(0b1010101010101010));
    EXPECT_EQ(val.load(), static_cast<short>(0b0110011001100110));
}

TEST(atomic_int16_test, fetch_xor_acq_rel)
{
    auto val = tempest::atomic<tempest::int16_t>{static_cast<short>(0b1010101010101010)};
    const auto old_val = val.fetch_xor(static_cast<short>(0b1100110011001100), tempest::memory_order::acq_rel);

    EXPECT_EQ(old_val, static_cast<short>(0b1010101010101010));
    EXPECT_EQ(val.load(), static_cast<short>(0b0110011001100110));
}

TEST(atomic_int16_test, fetch_xor_seq_cst)
{
    auto val = tempest::atomic<tempest::int16_t>{static_cast<short>(0b1010101010101010)};
    const auto old_val = val.fetch_xor(static_cast<short>(0b1100110011001100), tempest::memory_order::seq_cst);

    EXPECT_EQ(old_val, static_cast<short>(0b1010101010101010));
    EXPECT_EQ(val.load(), static_cast<short>(0b0110011001100110));
}

TEST(atomic_int32_test, fetch_xor)
{
    auto val = tempest::atomic<tempest::int32_t>{static_cast<int>(0b10101010101010101010101010101010)};
    const auto old_val = val.fetch_xor(static_cast<int>(0b11001100110011001100110011001100));

    EXPECT_EQ(old_val, static_cast<int>(0b10101010101010101010101010101010));
    EXPECT_EQ(val.load(), static_cast<int>(0b01100110011001100110011001100110));
}

TEST(atomic_int32_test, fetch_xor_relaxed)
{
    auto val = tempest::atomic<tempest::int32_t>{static_cast<int>(0b10101010101010101010101010101010)};
    const auto old_val =
        val.fetch_xor(static_cast<int>(0b11001100110011001100110011001100), tempest::memory_order::relaxed);

    EXPECT_EQ(old_val, static_cast<int>(0b10101010101010101010101010101010));
    EXPECT_EQ(val.load(), static_cast<int>(0b01100110011001100110011001100110));
}

TEST(atomic_int32_test, fetch_xor_acquire)
{
    auto val = tempest::atomic<tempest::int32_t>{static_cast<int>(0b10101010101010101010101010101010)};
    const auto old_val =
        val.fetch_xor(static_cast<int>(0b11001100110011001100110011001100), tempest::memory_order::acquire);

    EXPECT_EQ(old_val, static_cast<int>(0b10101010101010101010101010101010));
    EXPECT_EQ(val.load(), static_cast<int>(0b01100110011001100110011001100110));
}

TEST(atomic_int32_test, fetch_xor_acq_rel)
{
    auto val = tempest::atomic<tempest::int32_t>{static_cast<int>(0b10101010101010101010101010101010)};
    const auto old_val =
        val.fetch_xor(static_cast<int>(0b11001100110011001100110011001100), tempest::memory_order::acq_rel);

    EXPECT_EQ(old_val, static_cast<int>(0b10101010101010101010101010101010));
    EXPECT_EQ(val.load(), static_cast<int>(0b01100110011001100110011001100110));
}

TEST(atomic_int32_test, fetch_xor_seq_cst)
{
    auto val = tempest::atomic<tempest::int32_t>{static_cast<int>(0b10101010101010101010101010101010)};
    const auto old_val =
        val.fetch_xor(static_cast<int>(0b11001100110011001100110011001100), tempest::memory_order::seq_cst);

    EXPECT_EQ(old_val, static_cast<int>(0b10101010101010101010101010101010));
    EXPECT_EQ(val.load(), static_cast<int>(0b01100110011001100110011001100110));
}

TEST(atomic_int64_test, fetch_xor)
{
    auto val = tempest::atomic<tempest::int64_t>{
        static_cast<long long>(0b1010101010101010101010101010101010101010101010101010101010101010LL)};
    const auto old_val =
        val.fetch_xor(static_cast<long long>(0b1100110011001100110011001100110011001100110011001100110011001100LL));

    EXPECT_EQ(old_val, static_cast<long long>(0b1010101010101010101010101010101010101010101010101010101010101010LL));
    EXPECT_EQ(val.load(), static_cast<long long>(0b0110011001100110011001100110011001100110011001100110011001100110LL));
}

TEST(atomic_int64_test, fetch_xor_relaxed)
{
    auto val = tempest::atomic<tempest::int64_t>{
        static_cast<long long>(0b1010101010101010101010101010101010101010101010101010101010101010LL)};
    const auto old_val =
        val.fetch_xor(static_cast<long long>(0b1100110011001100110011001100110011001100110011001100110011001100LL),
                      tempest::memory_order::relaxed);

    EXPECT_EQ(old_val, static_cast<long long>(0b1010101010101010101010101010101010101010101010101010101010101010LL));
    EXPECT_EQ(val.load(), static_cast<long long>(0b0110011001100110011001100110011001100110011001100110011001100110LL));
}

TEST(atomic_int64_test, fetch_xor_acquire)
{
    auto val = tempest::atomic<tempest::int64_t>{
        static_cast<long long>(0b1010101010101010101010101010101010101010101010101010101010101010LL)};
    const auto old_val =
        val.fetch_xor(static_cast<long long>(0b1100110011001100110011001100110011001100110011001100110011001100LL),
                      tempest::memory_order::acquire);

    EXPECT_EQ(old_val, static_cast<long long>(0b1010101010101010101010101010101010101010101010101010101010101010LL));
    EXPECT_EQ(val.load(), static_cast<long long>(0b0110011001100110011001100110011001100110011001100110011001100110LL));
}

TEST(atomic_int64_test, fetch_xor_acq_rel)
{
    auto val = tempest::atomic<tempest::int64_t>{
        static_cast<long long>(0b1010101010101010101010101010101010101010101010101010101010101010LL)};
    const auto old_val =
        val.fetch_xor(static_cast<long long>(0b1100110011001100110011001100110011001100110011001100110011001100LL),
                      tempest::memory_order::acq_rel);

    EXPECT_EQ(old_val, static_cast<long long>(0b1010101010101010101010101010101010101010101010101010101010101010LL));
    EXPECT_EQ(val.load(), static_cast<long long>(0b0110011001100110011001100110011001100110011001100110011001100110LL));
}

TEST(atomic_int64_test, fetch_xor_seq_cst)
{
    auto val = tempest::atomic<tempest::int64_t>{
        static_cast<long long>(0b1010101010101010101010101010101010101010101010101010101010101010LL)};
    const auto old_val =
        val.fetch_xor(static_cast<long long>(0b1100110011001100110011001100110011001100110011001100110011001100LL),
                      tempest::memory_order::seq_cst);

    EXPECT_EQ(old_val, static_cast<long long>(0b1010101010101010101010101010101010101010101010101010101010101010LL));
    EXPECT_EQ(val.load(), static_cast<long long>(0b0110011001100110011001100110011001100110011001100110011001100110LL));
}

TEST(atomic_int8_test, wait_notify)
{
    auto val = tempest::atomic<tempest::int8_t>{0};
    auto resumed = tempest::atomic<tempest::int8_t>{0};
    auto waiting_started = tempest::atomic<tempest::int8_t>{1};

    auto thr = std::thread([&]() {
        (void)waiting_started.fetch_sub(1);
        
        while (val.load() == 0) {
            val.wait(0);
        }

        (void)resumed.fetch_add(1);
    });

    waiting_started.wait(0);
    val.store(1);
    val.notify_one();

    thr.join();

    EXPECT_EQ(resumed.load(), 1);
}

TEST(atomic_int8_test, wait_notify_all)
{
    auto val = tempest::atomic<tempest::int8_t>{0};
    auto resumed = tempest::atomic<tempest::int8_t>{0};
    auto waiting_started = tempest::atomic<tempest::int8_t>{2};

    auto thr1 = std::thread([&]() {
        (void)waiting_started.fetch_sub(1);
        
        while (val.load() == 0) {
            val.wait(0);
        }

        (void)resumed.fetch_add(1);
    });

    auto thr2 = std::thread([&]() {
        (void)waiting_started.fetch_sub(1);
        
        while (val.load() == 0) {
            val.wait(0);
        }

        (void)resumed.fetch_add(1);
    });

    waiting_started.wait(0);
    val.store(1);
    val.notify_all();

    thr1.join();
    thr2.join();

    EXPECT_EQ(resumed.load(), 2);
}

TEST(atomic_int16_test, wait_notify)
{
    auto val = tempest::atomic<tempest::int16_t>{0};
    auto resumed = tempest::atomic<tempest::int16_t>{0};
    auto waiting_started = tempest::atomic<tempest::int16_t>{1};

    auto thr = std::thread([&]() {
        (void)waiting_started.fetch_sub(1);
        
        while (val.load() == 0) {
            val.wait(0);
        }

        (void)resumed.fetch_add(1);
    });

    waiting_started.wait(0);
    val.store(1);
    val.notify_one();

    thr.join();

    EXPECT_EQ(resumed.load(), 1);
}

TEST(atomic_int16_test, wait_notify_all)
{
    auto val = tempest::atomic<tempest::int16_t>{0};
    auto resumed = tempest::atomic<tempest::int16_t>{0};
    auto waiting_started = tempest::atomic<tempest::int16_t>{2};

    auto thr1 = std::thread([&]() {
        (void)waiting_started.fetch_sub(1);
        
        while (val.load() == 0) {
            val.wait(0);
        }

        (void)resumed.fetch_add(1);
    });

    auto thr2 = std::thread([&]() {
        (void)waiting_started.fetch_sub(1);
        
        while (val.load() == 0) {
            val.wait(0);
        }

        (void)resumed.fetch_add(1);
    });

    waiting_started.wait(0);
    val.store(1);
    val.notify_all();

    thr1.join();
    thr2.join();

    EXPECT_EQ(resumed.load(), 2);
}

TEST(atomic_int32_test, wait_notify)
{
    auto val = tempest::atomic<tempest::int32_t>{0};
    auto resumed = tempest::atomic<tempest::int32_t>{0};
    auto waiting_started = tempest::atomic<tempest::int32_t>{1};

    auto thr = std::thread([&]() {
        (void)waiting_started.fetch_sub(1);
        
        while (val.load() == 0) {
            val.wait(0);
        }

        (void)resumed.fetch_add(1);
    });

    waiting_started.wait(0);
    val.store(1);
    val.notify_one();

    thr.join();

    EXPECT_EQ(resumed.load(), 1);
}

TEST(atomic_int32_test, wait_notify_all)
{
    auto val = tempest::atomic<tempest::int32_t>{0};
    auto resumed = tempest::atomic<tempest::int32_t>{0};
    auto waiting_started = tempest::atomic<tempest::int32_t>{2};

    auto thr1 = std::thread([&]() {
        (void)waiting_started.fetch_sub(1);
        
        while (val.load() == 0) {
            val.wait(0);
        }

        (void)resumed.fetch_add(1);
    });

    auto thr2 = std::thread([&]() {
        (void)waiting_started.fetch_sub(1);
        
        while (val.load() == 0) {
            val.wait(0);
        }

        (void)resumed.fetch_add(1);
    });

    waiting_started.wait(0);
    val.store(1);
    val.notify_all();

    thr1.join();
    thr2.join();

    EXPECT_EQ(resumed.load(), 2);
}

TEST(atomic_int64_test, wait_notify)
{
    auto val = tempest::atomic<tempest::int64_t>{0};
    auto resumed = tempest::atomic<tempest::int64_t>{0};
    auto waiting_started = tempest::atomic<tempest::int64_t>{1};

    auto thr = std::thread([&]() {
        (void)waiting_started.fetch_sub(1);
        
        while (val.load() == 0) {
            val.wait(0);
        }

        (void)resumed.fetch_add(1);
    });

    waiting_started.wait(0);
    val.store(1);
    val.notify_one();

    thr.join();

    EXPECT_EQ(resumed.load(), 1);
}

TEST(atomic_int64_test, wait_notify_all)
{
    auto val = tempest::atomic<tempest::int64_t>{0};
    auto resumed = tempest::atomic<tempest::int64_t>{0};
    auto waiting_started = tempest::atomic<tempest::int64_t>{2};

    auto thr1 = std::thread([&]() {
        (void)waiting_started.fetch_sub(1);
        
        while (val.load() == 0) {
            val.wait(0);
        }

        (void)resumed.fetch_add(1);
    });

    auto thr2 = std::thread([&]() {
        (void)waiting_started.fetch_sub(1);
        
        while (val.load() == 0) {
            val.wait(0);
        }

        (void)resumed.fetch_add(1);
    });

    waiting_started.wait(0);
    val.store(1);
    val.notify_all();

    thr1.join();
    thr2.join();

    EXPECT_EQ(resumed.load(), 2);
}

TEST(atomic_bool, value_construct)
{
    auto val = tempest::atomic<bool>{true};
    EXPECT_EQ(val.load(), true);
}

TEST(atomic_bool, store_and_load)
{
    auto val = tempest::atomic<bool>{false};
    EXPECT_EQ(val.load(), false);

    val.store(true);
    EXPECT_EQ(val.load(), true);

    val.store(false);
    EXPECT_EQ(val.load(), false);
}

TEST(atomic_bool, store_and_load_relaxed)
{
    auto val = tempest::atomic<bool>{false};
    EXPECT_EQ(val.load(tempest::memory_order::relaxed), false);

    val.store(true, tempest::memory_order::relaxed);
    EXPECT_EQ(val.load(tempest::memory_order::relaxed), true);

    val.store(false, tempest::memory_order::relaxed);
    EXPECT_EQ(val.load(tempest::memory_order::relaxed), false);
}

TEST(atomic_bool, store_release_and_load_acquire)
{
    auto val = tempest::atomic<bool>{false};
    EXPECT_EQ(val.load(tempest::memory_order::acquire), false);

    val.store(true, tempest::memory_order::release);
    EXPECT_EQ(val.load(tempest::memory_order::acquire), true);

    val.store(false, tempest::memory_order::release);
    EXPECT_EQ(val.load(tempest::memory_order::acquire), false);
}

TEST(atomic_bool, store_and_load_seq_cst)
{
    auto val = tempest::atomic<bool>{false};
    EXPECT_EQ(val.load(tempest::memory_order::seq_cst), false);

    val.store(true, tempest::memory_order::seq_cst);
    EXPECT_EQ(val.load(tempest::memory_order::seq_cst), true);

    val.store(false, tempest::memory_order::seq_cst);
    EXPECT_EQ(val.load(tempest::memory_order::seq_cst), false);
}

TEST(atomic_bool, exchange)
{
    auto val = tempest::atomic<bool>{false};
    const auto old_val = val.exchange(true);

    EXPECT_EQ(old_val, false);
    EXPECT_EQ(val.load(), true);
}

TEST(atomic_bool, exchange_relaxed)
{
    auto val = tempest::atomic<bool>{false};
    const auto old_val = val.exchange(true, tempest::memory_order::relaxed);

    EXPECT_EQ(old_val, false);
    EXPECT_EQ(val.load(tempest::memory_order::relaxed), true);
}

TEST(atomic_bool, exchange_acquire)
{
    auto val = tempest::atomic<bool>{false};
    const auto old_val = val.exchange(true, tempest::memory_order::acquire);

    EXPECT_EQ(old_val, false);
    EXPECT_EQ(val.load(tempest::memory_order::acquire), true);
}

TEST(atomic_bool, exchange_release)
{
    auto val = tempest::atomic<bool>{false};
    const auto old_val = val.exchange(true, tempest::memory_order::release);

    EXPECT_EQ(old_val, false);
    EXPECT_EQ(val.load(tempest::memory_order::acquire), true);
}

TEST(atomic_bool, exchange_acq_rel)
{
    auto val = tempest::atomic<bool>{false};
    const auto old_val = val.exchange(true, tempest::memory_order::acq_rel);

    EXPECT_EQ(old_val, false);
    EXPECT_EQ(val.load(tempest::memory_order::acquire), true);
}

TEST(atomic_bool, exchange_seq_cst)
{
    auto val = tempest::atomic<bool>{false};
    const auto old_val = val.exchange(true, tempest::memory_order::seq_cst);

    EXPECT_EQ(old_val, false);
    EXPECT_EQ(val.load(tempest::memory_order::seq_cst), true);
}

TEST(atomic_bool, compare_exchange_strong)
{
    auto val = tempest::atomic<bool>{false};
    auto expected = false;
    const auto result = val.compare_exchange_strong(expected, true);

    EXPECT_EQ(result, true);
    EXPECT_EQ(expected, false);
    EXPECT_EQ(val.load(), true);
}

TEST(atomic_bool, compare_exchange_strong_relaxed)
{
    auto val = tempest::atomic<bool>{false};
    auto expected = false;
    const auto result =
        val.compare_exchange_strong(expected, true, tempest::memory_order::relaxed);

    EXPECT_EQ(result, true);
    EXPECT_EQ(expected, false);
    EXPECT_EQ(val.load(tempest::memory_order::relaxed), true);
}

TEST(atomic_bool, compare_exchange_strong_acquire)
{
    auto val = tempest::atomic<bool>{false};
    auto expected = false;
    const auto result =
        val.compare_exchange_strong(expected, true, tempest::memory_order::acquire);

    EXPECT_EQ(result, true);
    EXPECT_EQ(expected, false);
    EXPECT_EQ(val.load(tempest::memory_order::acquire), true);
}

TEST(atomic_bool, compare_exchange_strong_release)
{
    auto val = tempest::atomic<bool>{false};
    auto expected = false;
    const auto result =
        val.compare_exchange_strong(expected, true, tempest::memory_order::release);

    EXPECT_EQ(result, true);
    EXPECT_EQ(expected, false);
    EXPECT_EQ(val.load(tempest::memory_order::acquire), true);
}

TEST(atomic_bool, compare_exchange_strong_acq_rel)
{
    auto val = tempest::atomic<bool>{false};
    auto expected = false;
    const auto result =
        val.compare_exchange_strong(expected, true, tempest::memory_order::acq_rel);

    EXPECT_EQ(result, true);
    EXPECT_EQ(expected, false);
    EXPECT_EQ(val.load(tempest::memory_order::acquire), true);
}

TEST(atomic_bool, compare_exchange_strong_seq_cst)
{
    auto val = tempest::atomic<bool>{false};
    auto expected = false;
    const auto result =
        val.compare_exchange_strong(expected, true, tempest::memory_order::seq_cst);

    EXPECT_EQ(result, true);
    EXPECT_EQ(expected, false);
    EXPECT_EQ(val.load(tempest::memory_order::seq_cst), true);
}

TEST(atomic_bool, fetch_and)
{
    auto val = tempest::atomic<bool>{false};
    const auto old_val = val.fetch_and(true);

    EXPECT_EQ(old_val, false);
    EXPECT_EQ(val.load(), false);
}

TEST(atomic_bool, fetch_and_relaxed)
{
    auto val = tempest::atomic<bool>{false};
    const auto old_val = val.fetch_and(true, tempest::memory_order::relaxed);

    EXPECT_EQ(old_val, false);
    EXPECT_EQ(val.load(tempest::memory_order::relaxed), false);
}

TEST(atomic_bool, fetch_and_acquire)
{
    auto val = tempest::atomic<bool>{false};
    const auto old_val = val.fetch_and(true, tempest::memory_order::acquire);

    EXPECT_EQ(old_val, false);
    EXPECT_EQ(val.load(tempest::memory_order::acquire), false);
}

TEST(atomic_bool, fetch_and_acq_rel)
{
    auto val = tempest::atomic<bool>{false};
    const auto old_val = val.fetch_and(true, tempest::memory_order::acq_rel);

    EXPECT_EQ(old_val, false);
    EXPECT_EQ(val.load(tempest::memory_order::acquire), false);
}

TEST(atomic_bool, fetch_and_seq_cst)
{
    auto val = tempest::atomic<bool>{false};
    const auto old_val = val.fetch_and(true, tempest::memory_order::seq_cst);

    EXPECT_EQ(old_val, false);
    EXPECT_EQ(val.load(tempest::memory_order::seq_cst), false);
}

TEST(atomic_bool, fetch_or)
{
    auto val = tempest::atomic<bool>{false};
    const auto old_val = val.fetch_or(true);

    EXPECT_EQ(old_val, false);
    EXPECT_EQ(val.load(), true);
}

TEST(atomic_bool, fetch_or_relaxed)
{
    auto val = tempest::atomic<bool>{false};
    const auto old_val = val.fetch_or(true, tempest::memory_order::relaxed);

    EXPECT_EQ(old_val, false);
    EXPECT_EQ(val.load(tempest::memory_order::relaxed), true);
}

TEST(atomic_bool, fetch_or_acquire)
{
    auto val = tempest::atomic<bool>{false};
    const auto old_val = val.fetch_or(true, tempest::memory_order::acquire);

    EXPECT_EQ(old_val, false);
    EXPECT_EQ(val.load(tempest::memory_order::acquire), true);
}

TEST(atomic_bool, fetch_or_acq_rel)
{
    auto val = tempest::atomic<bool>{false};
    const auto old_val = val.fetch_or(true, tempest::memory_order::acq_rel);

    EXPECT_EQ(old_val, false);
    EXPECT_EQ(val.load(tempest::memory_order::acquire), true);
}

TEST(atomic_bool, fetch_or_seq_cst)
{
    auto val = tempest::atomic<bool>{false};
    const auto old_val = val.fetch_or(true, tempest::memory_order::seq_cst);

    EXPECT_EQ(old_val, false);
    EXPECT_EQ(val.load(tempest::memory_order::seq_cst), true);
}

TEST(atomic_bool, fetch_xor)
{
    auto val = tempest::atomic<bool>{false};
    const auto old_val = val.fetch_xor(true);

    EXPECT_EQ(old_val, false);
    EXPECT_EQ(val.load(), true);
}

TEST(atomic_bool, fetch_xor_relaxed)
{
    auto val = tempest::atomic<bool>{false};
    const auto old_val = val.fetch_xor(true, tempest::memory_order::relaxed);

    EXPECT_EQ(old_val, false);
    EXPECT_EQ(val.load(tempest::memory_order::relaxed), true);
}

TEST(atomic_bool, fetch_xor_acquire)
{
    auto val = tempest::atomic<bool>{false};
    const auto old_val = val.fetch_xor(true, tempest::memory_order::acquire);

    EXPECT_EQ(old_val, false);
    EXPECT_EQ(val.load(tempest::memory_order::acquire), true);
}

TEST(atomic_bool, fetch_xor_acq_rel)
{
    auto val = tempest::atomic<bool>{false};
    const auto old_val = val.fetch_xor(true, tempest::memory_order::acq_rel);

    EXPECT_EQ(old_val, false);
    EXPECT_EQ(val.load(tempest::memory_order::acquire), true);
}

TEST(atomic_bool, fetch_xor_seq_cst)
{
    auto val = tempest::atomic<bool>{false};
    const auto old_val = val.fetch_xor(true, tempest::memory_order::seq_cst);

    EXPECT_EQ(old_val, false);
    EXPECT_EQ(val.load(tempest::memory_order::seq_cst), true);
}
