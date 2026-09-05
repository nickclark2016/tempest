#include <tempest/int.hpp>
#include <tempest/limits.hpp>
#include <tempest/math.hpp>

#include <limits>

#include <gtest/gtest.h>

TEST(tempest_limits, numeric_limits_float)
{
    // Test all the values of the numeric_limits<float> specialization against the standard library
    // numeric_limits<float> specialization.

    ASSERT_EQ(tempest::numeric_limits<float>::is_specialized, std::numeric_limits<float>::is_specialized);
    ASSERT_EQ(tempest::numeric_limits<float>::is_signed, std::numeric_limits<float>::is_signed);
    ASSERT_EQ(tempest::numeric_limits<float>::is_integer, std::numeric_limits<float>::is_integer);
    ASSERT_EQ(tempest::numeric_limits<float>::is_exact, std::numeric_limits<float>::is_exact);
    ASSERT_EQ(tempest::numeric_limits<float>::has_infinity, std::numeric_limits<float>::has_infinity);
    ASSERT_EQ(tempest::numeric_limits<float>::has_quiet_NaN, std::numeric_limits<float>::has_quiet_NaN);
    ASSERT_EQ(tempest::numeric_limits<float>::has_signaling_NaN, std::numeric_limits<float>::has_signaling_NaN);
    ASSERT_EQ(tempest::numeric_limits<float>::is_iec559, std::numeric_limits<float>::is_iec559);
    ASSERT_EQ(tempest::numeric_limits<float>::is_bounded, std::numeric_limits<float>::is_bounded);
    ASSERT_EQ(tempest::numeric_limits<float>::is_modulo, std::numeric_limits<float>::is_modulo);
    ASSERT_EQ(tempest::numeric_limits<float>::digits, std::numeric_limits<float>::digits);
    ASSERT_EQ(tempest::numeric_limits<float>::digits10, std::numeric_limits<float>::digits10);
    ASSERT_EQ(tempest::numeric_limits<float>::max_digits10, std::numeric_limits<float>::max_digits10);
    ASSERT_EQ(tempest::numeric_limits<float>::radix, std::numeric_limits<float>::radix);
    ASSERT_EQ(tempest::numeric_limits<float>::min_exponent, std::numeric_limits<float>::min_exponent);
    ASSERT_EQ(tempest::numeric_limits<float>::min_exponent10, std::numeric_limits<float>::min_exponent10);
    ASSERT_EQ(tempest::numeric_limits<float>::max_exponent, std::numeric_limits<float>::max_exponent);
    ASSERT_EQ(tempest::numeric_limits<float>::max_exponent10, std::numeric_limits<float>::max_exponent10);

    ASSERT_EQ(tempest::numeric_limits<float>::min(), std::numeric_limits<float>::min());
    ASSERT_EQ(tempest::numeric_limits<float>::max(), std::numeric_limits<float>::max());
    ASSERT_EQ(tempest::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest());
    ASSERT_EQ(tempest::numeric_limits<float>::epsilon(), std::numeric_limits<float>::epsilon());
    ASSERT_EQ(tempest::numeric_limits<float>::round_error(), std::numeric_limits<float>::round_error());
    ASSERT_EQ(tempest::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity());

    if constexpr (tempest::numeric_limits<float>::has_quiet_NaN)
    {
        ASSERT_TRUE(tempest::isnan(tempest::numeric_limits<float>::quiet_NaN()));
    }

    if constexpr (tempest::numeric_limits<float>::has_signaling_NaN)
    {
        ASSERT_TRUE(tempest::isnan(tempest::numeric_limits<float>::signaling_NaN()));
    }
}

TEST(tempest_limits, numeric_limits_double)
{
    // Test all the values of the numeric_limits<double> specialization against the standard library
    // numeric_limits<double> specialization.

    ASSERT_EQ(tempest::numeric_limits<double>::is_specialized, std::numeric_limits<double>::is_specialized);
    ASSERT_EQ(tempest::numeric_limits<double>::is_signed, std::numeric_limits<double>::is_signed);
    ASSERT_EQ(tempest::numeric_limits<double>::is_integer, std::numeric_limits<double>::is_integer);
    ASSERT_EQ(tempest::numeric_limits<double>::is_exact, std::numeric_limits<double>::is_exact);
    ASSERT_EQ(tempest::numeric_limits<double>::has_infinity, std::numeric_limits<double>::has_infinity);
    ASSERT_EQ(tempest::numeric_limits<double>::has_quiet_NaN, std::numeric_limits<double>::has_quiet_NaN);
    ASSERT_EQ(tempest::numeric_limits<double>::has_signaling_NaN, std::numeric_limits<double>::has_signaling_NaN);
    ASSERT_EQ(tempest::numeric_limits<double>::is_iec559, std::numeric_limits<double>::is_iec559);
    ASSERT_EQ(tempest::numeric_limits<double>::is_bounded, std::numeric_limits<double>::is_bounded);
    ASSERT_EQ(tempest::numeric_limits<double>::is_modulo, std::numeric_limits<double>::is_modulo);
    ASSERT_EQ(tempest::numeric_limits<double>::digits, std::numeric_limits<double>::digits);
    ASSERT_EQ(tempest::numeric_limits<double>::digits10, std::numeric_limits<double>::digits10);
    ASSERT_EQ(tempest::numeric_limits<double>::max_digits10, std::numeric_limits<double>::max_digits10);
    ASSERT_EQ(tempest::numeric_limits<double>::radix, std::numeric_limits<double>::radix);
    ASSERT_EQ(tempest::numeric_limits<double>::min_exponent, std::numeric_limits<double>::min_exponent);
    ASSERT_EQ(tempest::numeric_limits<double>::min_exponent10, std::numeric_limits<double>::min_exponent10);
    ASSERT_EQ(tempest::numeric_limits<double>::max_exponent, std::numeric_limits<double>::max_exponent);
    ASSERT_EQ(tempest::numeric_limits<double>::max_exponent10, std::numeric_limits<double>::max_exponent10);

    ASSERT_EQ(tempest::numeric_limits<double>::min(), std::numeric_limits<double>::min());
    ASSERT_EQ(tempest::numeric_limits<double>::max(), std::numeric_limits<double>::max());
    ASSERT_EQ(tempest::numeric_limits<double>::lowest(), std::numeric_limits<double>::lowest());
    ASSERT_EQ(tempest::numeric_limits<double>::epsilon(), std::numeric_limits<double>::epsilon());
    ASSERT_EQ(tempest::numeric_limits<double>::round_error(), std::numeric_limits<double>::round_error());
    ASSERT_EQ(tempest::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());

    if constexpr (tempest::numeric_limits<double>::has_quiet_NaN)
    {
        ASSERT_TRUE(tempest::isnan(tempest::numeric_limits<double>::quiet_NaN()));
    }

    if constexpr (tempest::numeric_limits<double>::has_signaling_NaN)
    {
        ASSERT_TRUE(tempest::isnan(tempest::numeric_limits<double>::signaling_NaN()));
    }
}

namespace
{
    template <typename T>
    void test_integral_numeric_limits()
    {
        ASSERT_EQ(tempest::numeric_limits<T>::is_specialized, std::numeric_limits<T>::is_specialized);
        ASSERT_EQ(tempest::numeric_limits<T>::is_signed, std::numeric_limits<T>::is_signed);
        ASSERT_EQ(tempest::numeric_limits<T>::is_integer, std::numeric_limits<T>::is_integer);
        ASSERT_EQ(tempest::numeric_limits<T>::is_exact, std::numeric_limits<T>::is_exact);
        ASSERT_EQ(tempest::numeric_limits<T>::has_infinity, std::numeric_limits<T>::has_infinity);
        ASSERT_EQ(tempest::numeric_limits<T>::has_quiet_NaN, std::numeric_limits<T>::has_quiet_NaN);
        ASSERT_EQ(tempest::numeric_limits<T>::has_signaling_NaN, std::numeric_limits<T>::has_signaling_NaN);
        ASSERT_EQ(tempest::numeric_limits<T>::is_iec559, std::numeric_limits<T>::is_iec559);
        ASSERT_EQ(tempest::numeric_limits<T>::is_bounded, std::numeric_limits<T>::is_bounded);
        ASSERT_EQ(tempest::numeric_limits<T>::is_modulo, std::numeric_limits<T>::is_modulo);
        ASSERT_EQ(tempest::numeric_limits<T>::digits, std::numeric_limits<T>::digits);
        ASSERT_EQ(tempest::numeric_limits<T>::digits10, std::numeric_limits<T>::digits10);
        ASSERT_EQ(tempest::numeric_limits<T>::max_digits10, std::numeric_limits<T>::max_digits10);
        ASSERT_EQ(tempest::numeric_limits<T>::radix, std::numeric_limits<T>::radix);
        ASSERT_EQ(tempest::numeric_limits<T>::min_exponent, std::numeric_limits<T>::min_exponent);
        ASSERT_EQ(tempest::numeric_limits<T>::min_exponent10, std::numeric_limits<T>::min_exponent10);
        ASSERT_EQ(tempest::numeric_limits<T>::max_exponent, std::numeric_limits<T>::max_exponent);
        ASSERT_EQ(tempest::numeric_limits<T>::max_exponent10, std::numeric_limits<T>::max_exponent10);

        ASSERT_EQ(tempest::numeric_limits<T>::min(), std::numeric_limits<T>::min());
        ASSERT_EQ(tempest::numeric_limits<T>::max(), std::numeric_limits<T>::max());
        ASSERT_EQ(tempest::numeric_limits<T>::lowest(), std::numeric_limits<T>::lowest());
        ASSERT_EQ(tempest::numeric_limits<T>::epsilon(), std::numeric_limits<T>::epsilon());
        ASSERT_EQ(tempest::numeric_limits<T>::round_error(), std::numeric_limits<T>::round_error());
        ASSERT_EQ(tempest::numeric_limits<T>::infinity(), std::numeric_limits<T>::infinity());
        ASSERT_EQ(tempest::numeric_limits<T>::quiet_NaN(), std::numeric_limits<T>::quiet_NaN());
        ASSERT_EQ(tempest::numeric_limits<T>::signaling_NaN(), std::numeric_limits<T>::signaling_NaN());
    }
} // namespace

/// @brief Verify numeric_limits<bool> properties against std::numeric_limits<bool>.
TEST(tempest_limits, numeric_limits_bool)
{
    test_integral_numeric_limits<bool>();
}

/// @brief Verify numeric_limits<int8_t> and numeric_limits<uint8_t> against standard library specializations.
TEST(tempest_limits, numeric_limits_8bit_integers)
{
    test_integral_numeric_limits<tempest::int8_t>();
    test_integral_numeric_limits<tempest::uint8_t>();
}

/// @brief Verify numeric_limits<int16_t> and numeric_limits<uint16_t> against standard library specializations.
TEST(tempest_limits, numeric_limits_16bit_integers)
{
    test_integral_numeric_limits<tempest::int16_t>();
    test_integral_numeric_limits<tempest::uint16_t>();
}

/// @brief Verify numeric_limits<int32_t> and numeric_limits<uint32_t> against standard library specializations.
TEST(tempest_limits, numeric_limits_32bit_integers)
{
    test_integral_numeric_limits<tempest::int32_t>();
    test_integral_numeric_limits<tempest::uint32_t>();
}

/// @brief Verify numeric_limits<int64_t> and numeric_limits<uint64_t> against standard library specializations.
TEST(tempest_limits, numeric_limits_64bit_integers)
{
    test_integral_numeric_limits<tempest::int64_t>();
    test_integral_numeric_limits<tempest::uint64_t>();
}
