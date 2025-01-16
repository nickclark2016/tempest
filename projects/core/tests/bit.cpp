#include <tempest/bit.hpp>

#include <gtest/gtest.h>

TEST(bit, has_single_bit_byte)
{
    using tempest::uint8_t;

    for (uint8_t i = 0; i < 8; ++i)
    {
        ASSERT_TRUE(tempest::has_single_bit(static_cast<uint8_t>(1 << i)));
    }

    // Test for non-power of two
    ASSERT_FALSE(tempest::has_single_bit(static_cast<uint8_t>(0b1010)));
    ASSERT_FALSE(tempest::has_single_bit(static_cast<uint8_t>(0b1100)));

    // Test for zero
    ASSERT_FALSE(tempest::has_single_bit(static_cast<uint8_t>(0)));
}

TEST(bit, has_single_bit_ushort)
{
    using tempest::uint16_t;

    for (uint16_t i = 0; i < 16; ++i)
    {
        ASSERT_TRUE(tempest::has_single_bit(static_cast<uint16_t>(1 << i)));
    }

    // Test for non-power of two
    ASSERT_FALSE(tempest::has_single_bit(static_cast<uint16_t>(0b1010)));
    ASSERT_FALSE(tempest::has_single_bit(static_cast<uint16_t>(0b1100)));

    // Test for zero
    ASSERT_FALSE(tempest::has_single_bit(static_cast<uint16_t>(0)));
}

TEST(bit, has_single_bit_uint)
{
    using tempest::uint32_t;

    for (uint32_t i = 0; i < 32; ++i)
    {
        ASSERT_TRUE(tempest::has_single_bit(static_cast<uint32_t>(1 << i)));
    }

    // Test for non-power of two
    ASSERT_FALSE(tempest::has_single_bit(static_cast<uint32_t>(0b1010)));
    ASSERT_FALSE(tempest::has_single_bit(static_cast<uint32_t>(0b1100)));

    // Test for zero
    ASSERT_FALSE(tempest::has_single_bit(static_cast<uint32_t>(0)));
}

TEST(bit, has_single_bit_ulong)
{
    using tempest::uint64_t;

    for (uint64_t i = 0; i < 64; ++i)
    {
        ASSERT_TRUE(tempest::has_single_bit(static_cast<uint64_t>(1) << i));
    }

    // Test for non-power of two
    ASSERT_FALSE(tempest::has_single_bit(static_cast<uint64_t>(0b1010)));
    ASSERT_FALSE(tempest::has_single_bit(static_cast<uint64_t>(0b1100)));

    // Test for zero
    ASSERT_FALSE(tempest::has_single_bit(static_cast<uint64_t>(0)));
}

TEST(bit, countl_zero_byte)
{
    using tempest::uint8_t;

    for (uint8_t i = 0; i < 8; ++i)
    {
        ASSERT_EQ(7 - i, tempest::countl_zero(static_cast<uint8_t>(1 << i)));
    }

    // Test for zero
    ASSERT_EQ(8, tempest::countl_zero(static_cast<uint8_t>(0)));
}

TEST(bit, countl_zero_ushort)
{
    using tempest::uint16_t;

    for (uint16_t i = 0; i < 16; ++i)
    {
        ASSERT_EQ(15 - i, tempest::countl_zero(static_cast<uint16_t>(1 << i)));
    }

    // Test for zero
    ASSERT_EQ(16, tempest::countl_zero(static_cast<uint16_t>(0)));
}

TEST(bit, countl_zero_uint)
{
    using tempest::uint32_t;

    for (uint32_t i = 0; i < 32; ++i)
    {
        ASSERT_EQ(31 - i, tempest::countl_zero(static_cast<uint32_t>(1 << i)));
    }

    // Test for zero
    ASSERT_EQ(32, tempest::countl_zero(static_cast<uint32_t>(0)));
}

TEST(bit, countl_zero_ulong)
{
    using tempest::uint64_t;

    for (uint64_t i = 0; i < 64; ++i)
    {
        ASSERT_EQ(63 - i, tempest::countl_zero(static_cast<uint64_t>(1) << i));
    }

    // Test for zero
    ASSERT_EQ(64, tempest::countl_zero(static_cast<uint64_t>(0)));
}

TEST(bit, countl_one_byte)
{
    using tempest::uint8_t;

    for (uint8_t i = 0; i < 8; ++i)
    {
        ASSERT_EQ(7 - i, tempest::countl_one(static_cast<uint8_t>(~(1 << i))));
    }

    // Test for zero
    ASSERT_EQ(8, tempest::countl_one(static_cast<uint8_t>(0xFF)));
}

TEST(bit, countl_one_ushort)
{
    using tempest::uint16_t;

    for (uint16_t i = 0; i < 16; ++i)
    {
        ASSERT_EQ(15 - i, tempest::countl_one(static_cast<uint16_t>(~(1 << i))));
    }

    // Test for zero
    ASSERT_EQ(16, tempest::countl_one(static_cast<uint16_t>(0xFFFF)));
}


TEST(bit, countl_one_uint)
{
    using tempest::uint32_t;

    for (uint32_t i = 0; i < 32; ++i)
    {
        ASSERT_EQ(31 - i, tempest::countl_one(static_cast<uint32_t>(~(1 << i))));
    }

    // Test for zero
    ASSERT_EQ(32, tempest::countl_one(static_cast<uint32_t>(0xFFFFFFFF)));
}

TEST(bit, countl_one_ulong)
{
    using tempest::uint64_t;

    for (uint64_t i = 0; i < 64; ++i)
    {
        uint64_t tmp = ~static_cast<uint64_t>(1);
        ASSERT_EQ(63 - i, tempest::countl_one(static_cast<uint64_t>(tmp << i)));
    }

    // Test for zero
    ASSERT_EQ(64, tempest::countl_one(static_cast<uint64_t>(0xFFFFFFFFFFFFFFFF)));
}

TEST(bit, countr_zero_byte)
{
    using tempest::uint8_t;

    for (uint8_t i = 0; i < 8; ++i)
    {
        ASSERT_EQ(i, tempest::countr_zero(static_cast<uint8_t>(1 << i)));
    }

    // Test for zero
    ASSERT_EQ(8, tempest::countr_zero(static_cast<uint8_t>(0)));
}

TEST(bit, countr_zero_ushort)
{
    using tempest::uint16_t;

    for (uint16_t i = 0; i < 16; ++i)
    {
        ASSERT_EQ(i, tempest::countr_zero(static_cast<uint16_t>(1 << i)));
    }

    // Test for zero
    ASSERT_EQ(16, tempest::countr_zero(static_cast<uint16_t>(0)));
}

TEST(bit, countr_zero_uint)
{
    using tempest::uint32_t;

    for (uint32_t i = 0; i < 32; ++i)
    {
        ASSERT_EQ(i, tempest::countr_zero(static_cast<uint32_t>(1 << i)));
    }

    // Test for zero
    ASSERT_EQ(32, tempest::countr_zero(static_cast<uint32_t>(0)));
}

TEST(bit, countr_zero_ulong)
{
    using tempest::uint64_t;

    for (uint64_t i = 0; i < 64; ++i)
    {
        ASSERT_EQ(i, tempest::countr_zero(static_cast<uint64_t>(1) << i));
    }

    // Test for zero
    ASSERT_EQ(64, tempest::countr_zero(static_cast<uint64_t>(0)));
}

TEST(bit, countr_one_byte)
{
    using tempest::uint8_t;

    for (uint8_t i = 0; i < 8; ++i)
    {
        ASSERT_EQ(i, tempest::countr_one(static_cast<uint8_t>(~(1 << i))));
    }

    // Test for zero
    ASSERT_EQ(8, tempest::countr_one(static_cast<uint8_t>(0xFF)));
}

TEST(bit, countr_one_ushort)
{
    using tempest::uint16_t;

    for (uint16_t i = 0; i < 16; ++i)
    {
        ASSERT_EQ(i, tempest::countr_one(static_cast<uint16_t>(~(1 << i))));
    }

    // Test for zero
    ASSERT_EQ(16, tempest::countr_one(static_cast<uint16_t>(0xFFFF)));
}

TEST(bit, countr_one_uint)
{
    using tempest::uint32_t;

    for (uint32_t i = 0; i < 32; ++i)
    {
        ASSERT_EQ(i, tempest::countr_one(static_cast<uint32_t>(~(1 << i))));
    }

    // Test for zero
    ASSERT_EQ(32, tempest::countr_one(static_cast<uint32_t>(0xFFFFFFFF)));
}

TEST(bit, countr_one_ulong)
{
    using tempest::uint64_t;

    for (uint64_t i = 0; i < 64; ++i)
    {
        uint64_t one = 1;
        uint64_t shifted = one << i;
        ASSERT_EQ(i, tempest::countr_one(static_cast<uint64_t>(~shifted)));
    }

    // Test for zero
    ASSERT_EQ(64, tempest::countr_one(static_cast<uint64_t>(0xFFFFFFFFFFFFFFFF)));
}