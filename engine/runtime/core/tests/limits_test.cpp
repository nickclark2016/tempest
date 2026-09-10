#include <tempest/int.hpp>
#include <tempest/limits.hpp>
#include <tempest/math.hpp>
#include <tempest/type_traits.hpp>

#include <gtest/gtest.h>

//=============================================================================
// Floating-Point Limits Tests
//=============================================================================

/// @brief Verify numeric_limits<float> properties against literal ground-truth values.
TEST(tempest_limits, numeric_limits_float)
{
    // 1. Assert: Type traits and representation properties
    ASSERT_TRUE(tempest::numeric_limits<float>::is_specialized);
    ASSERT_TRUE(tempest::numeric_limits<float>::is_signed);
    ASSERT_FALSE(tempest::numeric_limits<float>::is_integer);
    ASSERT_FALSE(tempest::numeric_limits<float>::is_exact);
    ASSERT_TRUE(tempest::numeric_limits<float>::has_infinity);
    ASSERT_TRUE(tempest::numeric_limits<float>::has_quiet_NaN);
    ASSERT_TRUE(tempest::numeric_limits<float>::has_signaling_NaN);
    ASSERT_TRUE(tempest::numeric_limits<float>::is_iec559);
    ASSERT_TRUE(tempest::numeric_limits<float>::is_bounded);
    ASSERT_FALSE(tempest::numeric_limits<float>::is_modulo);
    ASSERT_EQ(tempest::numeric_limits<float>::digits, 24);
    ASSERT_EQ(tempest::numeric_limits<float>::digits10, 6);
    ASSERT_EQ(tempest::numeric_limits<float>::max_digits10, 9);
    ASSERT_EQ(tempest::numeric_limits<float>::radix, 2);
    ASSERT_EQ(tempest::numeric_limits<float>::min_exponent, -125);
    ASSERT_EQ(tempest::numeric_limits<float>::min_exponent10, -37);
    ASSERT_EQ(tempest::numeric_limits<float>::max_exponent, 128);
    ASSERT_EQ(tempest::numeric_limits<float>::max_exponent10, 38);

    // 2. Assert: Boundary values, precision, and special floating-point values
    ASSERT_EQ(tempest::numeric_limits<float>::min(), 1.17549435e-38F);
    ASSERT_EQ(tempest::numeric_limits<float>::max(), 3.40282347e+38F);
    ASSERT_EQ(tempest::numeric_limits<float>::lowest(), -3.40282347e+38F);
    ASSERT_EQ(tempest::numeric_limits<float>::epsilon(), 1.19209290e-7F);
    ASSERT_EQ(tempest::numeric_limits<float>::round_error(), 0.5F);
    ASSERT_TRUE(tempest::isinf(tempest::numeric_limits<float>::infinity()));

    if constexpr (tempest::numeric_limits<float>::has_quiet_NaN)
    {
        ASSERT_TRUE(tempest::isnan(tempest::numeric_limits<float>::quiet_NaN()));
    }

    if constexpr (tempest::numeric_limits<float>::has_signaling_NaN)
    {
        ASSERT_TRUE(tempest::isnan(tempest::numeric_limits<float>::signaling_NaN()));
    }
}

/// @brief Verify numeric_limits<double> properties against literal ground-truth values.
TEST(tempest_limits, numeric_limits_double)
{
    // 1. Assert: Type traits and representation properties
    ASSERT_TRUE(tempest::numeric_limits<double>::is_specialized);
    ASSERT_TRUE(tempest::numeric_limits<double>::is_signed);
    ASSERT_FALSE(tempest::numeric_limits<double>::is_integer);
    ASSERT_FALSE(tempest::numeric_limits<double>::is_exact);
    ASSERT_TRUE(tempest::numeric_limits<double>::has_infinity);
    ASSERT_TRUE(tempest::numeric_limits<double>::has_quiet_NaN);
    ASSERT_TRUE(tempest::numeric_limits<double>::has_signaling_NaN);
    ASSERT_TRUE(tempest::numeric_limits<double>::is_iec559);
    ASSERT_TRUE(tempest::numeric_limits<double>::is_bounded);
    ASSERT_FALSE(tempest::numeric_limits<double>::is_modulo);
    ASSERT_EQ(tempest::numeric_limits<double>::digits, 53);
    ASSERT_EQ(tempest::numeric_limits<double>::digits10, 15);
    ASSERT_EQ(tempest::numeric_limits<double>::max_digits10, 17);
    ASSERT_EQ(tempest::numeric_limits<double>::radix, 2);
    ASSERT_EQ(tempest::numeric_limits<double>::min_exponent, -1021);
    ASSERT_EQ(tempest::numeric_limits<double>::min_exponent10, -307);
    ASSERT_EQ(tempest::numeric_limits<double>::max_exponent, 1024);
    ASSERT_EQ(tempest::numeric_limits<double>::max_exponent10, 308);

    // 2. Assert: Boundary values, precision, and special floating-point values
    ASSERT_EQ(tempest::numeric_limits<double>::min(), 2.2250738585072014e-308);
    ASSERT_EQ(tempest::numeric_limits<double>::max(), 1.7976931348623157e+308);
    ASSERT_EQ(tempest::numeric_limits<double>::lowest(), -1.7976931348623157e+308);
    ASSERT_EQ(tempest::numeric_limits<double>::epsilon(), 2.2204460492503131e-16);
    ASSERT_EQ(tempest::numeric_limits<double>::round_error(), 0.5);
    ASSERT_TRUE(tempest::isinf(tempest::numeric_limits<double>::infinity()));

    if constexpr (tempest::numeric_limits<double>::has_quiet_NaN)
    {
        ASSERT_TRUE(tempest::isnan(tempest::numeric_limits<double>::quiet_NaN()));
    }

    if constexpr (tempest::numeric_limits<double>::has_signaling_NaN)
    {
        ASSERT_TRUE(tempest::isnan(tempest::numeric_limits<double>::signaling_NaN()));
    }
}

//=============================================================================
// Integral Limits Tests
//=============================================================================

namespace
{
    template <typename T>
    void test_integral_numeric_limits()
    {
        ASSERT_TRUE(tempest::numeric_limits<T>::is_specialized);
        ASSERT_EQ(tempest::numeric_limits<T>::is_signed, tempest::is_signed_v<T>);
        ASSERT_TRUE(tempest::numeric_limits<T>::is_integer);
        ASSERT_TRUE(tempest::numeric_limits<T>::is_exact);
        ASSERT_FALSE(tempest::numeric_limits<T>::has_infinity);
        ASSERT_FALSE(tempest::numeric_limits<T>::has_quiet_NaN);
        ASSERT_FALSE(tempest::numeric_limits<T>::has_signaling_NaN);
        ASSERT_FALSE(tempest::numeric_limits<T>::is_iec559);
        ASSERT_TRUE(tempest::numeric_limits<T>::is_bounded);
        if constexpr (tempest::is_same_v<T, bool>)
        {
            ASSERT_FALSE(tempest::numeric_limits<T>::is_modulo);
        }
        else
        {
            ASSERT_EQ(tempest::numeric_limits<T>::is_modulo, !tempest::is_signed_v<T>);
        }
        ASSERT_EQ(tempest::numeric_limits<T>::max_digits10, 0);
        ASSERT_EQ(tempest::numeric_limits<T>::radix, 2);
        ASSERT_EQ(tempest::numeric_limits<T>::min_exponent, 0);
        ASSERT_EQ(tempest::numeric_limits<T>::min_exponent10, 0);
        ASSERT_EQ(tempest::numeric_limits<T>::max_exponent, 0);
        ASSERT_EQ(tempest::numeric_limits<T>::max_exponent10, 0);

        ASSERT_EQ(tempest::numeric_limits<T>::epsilon(), T{0});
        ASSERT_EQ(tempest::numeric_limits<T>::round_error(), T{0});
        ASSERT_EQ(tempest::numeric_limits<T>::infinity(), T{0});
        ASSERT_EQ(tempest::numeric_limits<T>::quiet_NaN(), T{0});
        ASSERT_EQ(tempest::numeric_limits<T>::signaling_NaN(), T{0});
    }
} // namespace

/// @brief Verify numeric_limits<bool> properties against literal ground-truth values.
TEST(tempest_limits, numeric_limits_bool)
{
    // 1. Act & Assert: Common integral properties
    test_integral_numeric_limits<bool>();

    // 2. Act & Assert: Type-specific bounds and digit precision
    ASSERT_EQ(tempest::numeric_limits<bool>::digits, 1);
    ASSERT_EQ(tempest::numeric_limits<bool>::digits10, 0);
    ASSERT_EQ(tempest::numeric_limits<bool>::min(), false);
    ASSERT_EQ(tempest::numeric_limits<bool>::lowest(), false);
    ASSERT_EQ(tempest::numeric_limits<bool>::max(), true);
}

/// @brief Verify numeric_limits<int8_t> and numeric_limits<uint8_t> against literal ground-truth values.
TEST(tempest_limits, numeric_limits_8bit_integers)
{
    // 1. Act & Assert: int8_t
    test_integral_numeric_limits<tempest::int8_t>();
    ASSERT_EQ(tempest::numeric_limits<tempest::int8_t>::digits, 7);
    ASSERT_EQ(tempest::numeric_limits<tempest::int8_t>::digits10, 2);
    ASSERT_EQ(tempest::numeric_limits<tempest::int8_t>::min(), -128);
    ASSERT_EQ(tempest::numeric_limits<tempest::int8_t>::lowest(), -128);
    ASSERT_EQ(tempest::numeric_limits<tempest::int8_t>::max(), 127);

    // 2. Act & Assert: uint8_t
    test_integral_numeric_limits<tempest::uint8_t>();
    ASSERT_EQ(tempest::numeric_limits<tempest::uint8_t>::digits, 8);
    ASSERT_EQ(tempest::numeric_limits<tempest::uint8_t>::digits10, 2);
    ASSERT_EQ(tempest::numeric_limits<tempest::uint8_t>::min(), 0);
    ASSERT_EQ(tempest::numeric_limits<tempest::uint8_t>::lowest(), 0);
    ASSERT_EQ(tempest::numeric_limits<tempest::uint8_t>::max(), 255);
}

/// @brief Verify numeric_limits<int16_t> and numeric_limits<uint16_t> against literal ground-truth values.
TEST(tempest_limits, numeric_limits_16bit_integers)
{
    // 1. Act & Assert: int16_t
    test_integral_numeric_limits<tempest::int16_t>();
    ASSERT_EQ(tempest::numeric_limits<tempest::int16_t>::digits, 15);
    ASSERT_EQ(tempest::numeric_limits<tempest::int16_t>::digits10, 4);
    ASSERT_EQ(tempest::numeric_limits<tempest::int16_t>::min(), -32768);
    ASSERT_EQ(tempest::numeric_limits<tempest::int16_t>::lowest(), -32768);
    ASSERT_EQ(tempest::numeric_limits<tempest::int16_t>::max(), 32767);

    // 2. Act & Assert: uint16_t
    test_integral_numeric_limits<tempest::uint16_t>();
    ASSERT_EQ(tempest::numeric_limits<tempest::uint16_t>::digits, 16);
    ASSERT_EQ(tempest::numeric_limits<tempest::uint16_t>::digits10, 4);
    ASSERT_EQ(tempest::numeric_limits<tempest::uint16_t>::min(), 0);
    ASSERT_EQ(tempest::numeric_limits<tempest::uint16_t>::lowest(), 0);
    ASSERT_EQ(tempest::numeric_limits<tempest::uint16_t>::max(), 65535);
}

/// @brief Verify numeric_limits<int32_t> and numeric_limits<uint32_t> against literal ground-truth values.
TEST(tempest_limits, numeric_limits_32bit_integers)
{
    // 1. Act & Assert: int32_t
    test_integral_numeric_limits<tempest::int32_t>();
    ASSERT_EQ(tempest::numeric_limits<tempest::int32_t>::digits, 31);
    ASSERT_EQ(tempest::numeric_limits<tempest::int32_t>::digits10, 9);
    ASSERT_EQ(tempest::numeric_limits<tempest::int32_t>::min(), -2147483648);
    ASSERT_EQ(tempest::numeric_limits<tempest::int32_t>::lowest(), -2147483648);
    ASSERT_EQ(tempest::numeric_limits<tempest::int32_t>::max(), 2147483647);

    // 2. Act & Assert: uint32_t
    test_integral_numeric_limits<tempest::uint32_t>();
    ASSERT_EQ(tempest::numeric_limits<tempest::uint32_t>::digits, 32);
    ASSERT_EQ(tempest::numeric_limits<tempest::uint32_t>::digits10, 9);
    ASSERT_EQ(tempest::numeric_limits<tempest::uint32_t>::min(), 0U);
    ASSERT_EQ(tempest::numeric_limits<tempest::uint32_t>::lowest(), 0U);
    ASSERT_EQ(tempest::numeric_limits<tempest::uint32_t>::max(), 4294967295U);
}

/// @brief Verify numeric_limits<int64_t> and numeric_limits<uint64_t> against literal ground-truth values.
TEST(tempest_limits, numeric_limits_64bit_integers)
{
    // 1. Act & Assert: int64_t
    test_integral_numeric_limits<tempest::int64_t>();
    ASSERT_EQ(tempest::numeric_limits<tempest::int64_t>::digits, 63);
    ASSERT_EQ(tempest::numeric_limits<tempest::int64_t>::digits10, 18);
    ASSERT_EQ(tempest::numeric_limits<tempest::int64_t>::min(), -9223372036854775807LL - 1);
    ASSERT_EQ(tempest::numeric_limits<tempest::int64_t>::lowest(), -9223372036854775807LL - 1);
    ASSERT_EQ(tempest::numeric_limits<tempest::int64_t>::max(), 9223372036854775807LL);

    // 2. Act & Assert: uint64_t
    test_integral_numeric_limits<tempest::uint64_t>();
    ASSERT_EQ(tempest::numeric_limits<tempest::uint64_t>::digits, 64);
    ASSERT_EQ(tempest::numeric_limits<tempest::uint64_t>::digits10, 19);
    ASSERT_EQ(tempest::numeric_limits<tempest::uint64_t>::min(), 0ULL);
    ASSERT_EQ(tempest::numeric_limits<tempest::uint64_t>::lowest(), 0ULL);
    ASSERT_EQ(tempest::numeric_limits<tempest::uint64_t>::max(), 18446744073709551615ULL);
}

/// @brief Verify numeric_limits<uintptr_t> and the named constant uintptr_max.
TEST(tempest_limits, numeric_limits_uintptr)
{
    // 1. Act & Assert: Common integral properties for uintptr_t
    test_integral_numeric_limits<tempest::uintptr_t>();

    // 2. Act & Assert: Named constant and maximum value consistency
    static_assert(tempest::uintptr_max == (~tempest::uintptr_t{0}));
    ASSERT_EQ(tempest::numeric_limits<tempest::uintptr_t>::min(), 0U);
    ASSERT_EQ(tempest::numeric_limits<tempest::uintptr_t>::max(), tempest::uintptr_max);
}
