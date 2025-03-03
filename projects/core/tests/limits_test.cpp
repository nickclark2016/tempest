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
