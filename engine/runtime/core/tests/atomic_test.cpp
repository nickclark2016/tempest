#include <gtest/gtest.h>

#include <tempest/atomic.hpp>

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
