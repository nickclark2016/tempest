#include <tempest/charconv.hpp>

#include <gtest/gtest.h>
#include <tempest/math_utils.hpp>
#include <tempest/string_view.hpp>

//=============================================================================
// Integer to_chars Tests
//=============================================================================

/// @brief Tests basic decimal integer conversions for signed and unsigned types.
TEST(charconv_test, integer_decimal_basic)
{
    // 1. Setup
    char buf[64];

    // 2. Act & Assert: zero
    auto res = tempest::to_chars(buf, buf + sizeof(buf), 0);
    EXPECT_TRUE(res);
    EXPECT_EQ(tempest::string_view(buf, res.ptr - buf), "0");

    // 3. Act & Assert: positive
    res = tempest::to_chars(buf, buf + sizeof(buf), 42);
    EXPECT_TRUE(res);
    EXPECT_EQ(tempest::string_view(buf, res.ptr - buf), "42");

    // 4. Act & Assert: negative
    res = tempest::to_chars(buf, buf + sizeof(buf), -12345);
    EXPECT_TRUE(res);
    EXPECT_EQ(tempest::string_view(buf, res.ptr - buf), "-12345");
}

/// @brief Tests integer boundary values (INT64_MIN, INT64_MAX, UINT64_MAX).
TEST(charconv_test /*unused*/, integer_boundary_values /*unused*/)
{
    // 1. Setup
    char buf[64];

    // 2. Act & Assert: INT64_MIN
    auto res = tempest::to_chars(buf, buf + sizeof(buf), tempest::numeric_limits<int64_t>::min());
    EXPECT_TRUE(res);
    EXPECT_EQ(tempest::string_view(buf, res.ptr - buf), "-9223372036854775808");

    // 3. Act & Assert: INT64_MAX
    res = tempest::to_chars(buf, buf + sizeof(buf), tempest::numeric_limits<int64_t>::max());
    EXPECT_TRUE(res);
    EXPECT_EQ(tempest::string_view(buf, res.ptr - buf), "9223372036854775807");

    // 4. Act & Assert: UINT64_MAX
    res = tempest::to_chars(buf, buf + sizeof(buf), tempest::numeric_limits<uint64_t>::max());
    EXPECT_TRUE(res);
    EXPECT_EQ(tempest::string_view(buf, res.ptr - buf), "18446744073709551615");
}

/// @brief Tests non-decimal bases (binary, octal, hexadecimal).
TEST(charconv_test /*unused*/, integer_radix_bases /*unused*/)
{
    // 1. Setup
    char buf[64];

    // 2. Act & Assert: Base 2
    auto res = tempest::to_chars(buf, buf + sizeof(buf), 42, 2);
    EXPECT_TRUE(res);
    EXPECT_EQ(tempest::string_view(buf, res.ptr - buf), "101010");

    // 3. Act & Assert: Base 8
    res = tempest::to_chars(buf, buf + sizeof(buf), 42, 8);
    EXPECT_TRUE(res);
    EXPECT_EQ(tempest::string_view(buf, res.ptr - buf), "52");

    // 4. Act & Assert: Base 16
    res = tempest::to_chars(buf, buf + sizeof(buf), 255, 16);
    EXPECT_TRUE(res);
    EXPECT_EQ(tempest::string_view(buf, res.ptr - buf), "ff");

    // 5. Act & Assert: Invalid base
    res = tempest::to_chars(buf, buf + sizeof(buf), 42, 1);
    EXPECT_FALSE(res);
    EXPECT_EQ(res.ec, tempest::errc::invalid_argument);
}

/// @brief Tests buffer overrun protection when output buffer is too small.
TEST(charconv_test /*unused*/, integer_buffer_too_small /*unused*/)
{
    // 1. Setup: Buffer only fits 2 characters
    char buf[2];

    // 2. Act: Attempt to write "123" into 2-char buffer
    auto res = tempest::to_chars(buf, buf + 2, 123);

    // 3. Assert: value_too_large error returned
    EXPECT_FALSE(res);
    EXPECT_EQ(res.ec, tempest::errc::value_too_large);
}

//=============================================================================
// Floating Point to_chars Tests
//=============================================================================

/// @brief Tests floating-point fixed-format representation with specific precision.
TEST(charconv_test /*unused*/, float_fixed_format /*unused*/)
{
    // 1. Setup
    char buf[64];

    // 2. Act & Assert: Fixed with 2 decimal places
    auto res = tempest::to_chars(buf, buf + sizeof(buf), tempest::math::constants::pi<double>,
                                 tempest::chars_format::fixed, 2);
    EXPECT_TRUE(res);
    EXPECT_EQ(tempest::string_view(buf, res.ptr - buf), "3.14");

    // 3. Act & Assert: Fixed with rounding up
    res = tempest::to_chars(buf, buf + sizeof(buf), 3.149, tempest::chars_format::fixed, 2);
    EXPECT_TRUE(res);
    EXPECT_EQ(tempest::string_view(buf, res.ptr - buf), "3.15");

    // 4. Act & Assert: Fixed zero precision
    res = tempest::to_chars(buf, buf + sizeof(buf), 42.7, tempest::chars_format::fixed, 0);
    EXPECT_TRUE(res);
    EXPECT_EQ(tempest::string_view(buf, res.ptr - buf), "43");
}

/// @brief Tests floating-point special values (NaN, Infinity, Negative Zero).
TEST(charconv_test /*unused*/, float_special_values /*unused*/)
{
    // 1. Setup
    char buf[64];

    // 2. Act & Assert: NaN
    auto nan_val = tempest::numeric_limits<float>::quiet_NaN();
    auto res = tempest::to_chars(buf, buf + sizeof(buf), nan_val);
    EXPECT_TRUE(res);
    EXPECT_EQ(tempest::string_view(buf, res.ptr - buf), "nan");

    // 3. Act & Assert: Infinity
    auto inf_val = tempest::numeric_limits<float>::infinity();
    res = tempest::to_chars(buf, buf + sizeof(buf), inf_val);
    EXPECT_TRUE(res);
    EXPECT_EQ(tempest::string_view(buf, res.ptr - buf), "inf");

    // 4. Act & Assert: Negative Infinity
    res = tempest::to_chars(buf, buf + sizeof(buf), -inf_val);
    EXPECT_TRUE(res);
    EXPECT_EQ(tempest::string_view(buf, res.ptr - buf), "-inf");

    // 5. Act & Assert: Negative Zero
    res = tempest::to_chars(buf, buf + sizeof(buf), -0.0F, tempest::chars_format::fixed, 1);
    EXPECT_TRUE(res);
    EXPECT_EQ(tempest::string_view(buf, res.ptr - buf), "-0.0");
}

/// @brief Tests floating-point scientific notation formatting.
TEST(charconv_test /*unused*/, float_scientific_format /*unused*/)
{
    // 1. Setup
    char buf[64];

    // 2. Act & Assert: Scientific notation
    auto res = tempest::to_chars(buf, buf + sizeof(buf), 12345.0, tempest::chars_format::scientific, 2);
    EXPECT_TRUE(res);
    EXPECT_EQ(tempest::string_view(buf, res.ptr - buf), "1.23e+04");

    // 3. Act & Assert: Small number scientific notation
    res = tempest::to_chars(buf, buf + sizeof(buf), 0.00125, tempest::chars_format::scientific, 2);
    EXPECT_TRUE(res);
    EXPECT_EQ(tempest::string_view(buf, res.ptr - buf), "1.25e-03");
}

/// @brief Tests floating-point general format with trailing zero trimming.
TEST(charconv_test /*unused*/, float_general_format /*unused*/)
{
    // 1. Setup
    char buf[64];

    // 2. Act & Assert: General format trims trailing zeros
    auto res = tempest::to_chars(buf, buf + sizeof(buf), 1.5, tempest::chars_format::general);
    EXPECT_TRUE(res);
    EXPECT_EQ(tempest::string_view(buf, res.ptr - buf), "1.5");

    // 3. Act & Assert: Exact integer float omits decimal point in general format
    res = tempest::to_chars(buf, buf + sizeof(buf), 100.0, tempest::chars_format::general);
    EXPECT_TRUE(res);
    EXPECT_EQ(tempest::string_view(buf, res.ptr - buf), "100");
}
