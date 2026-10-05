#include <tempest/half.hpp>

#include <tempest/bit.hpp>
#include <tempest/limits.hpp>
#include <tempest/math.hpp>

#include <gtest/gtest.h>

using tempest::bit_cast;
using tempest::float_to_half;
using tempest::half_to_float;
using tempest::is_inf_half;
using tempest::is_nan_half;
using tempest::uint16_t;
using tempest::uint32_t;

    // =========================================================================
    // SECTION: Zero and Sign Representation Tests
    // =========================================================================

    /// @brief Verifies that +0.0f and -0.0f convert to their respective half representations with sign preserved.
    TEST(half_tests, half_zero_representation)
    {
        // 1. Setup: Positive and negative single-precision floating-point zeros
        constexpr auto positive_zero = 0.0f;
        constexpr auto negative_zero = -0.0f;

        // 2. Act: Convert to 16-bit half precision
        const auto half_pos_zero = float_to_half(positive_zero);
        const auto half_neg_zero = float_to_half(negative_zero);

        // 3. Assert: Sign bit is preserved and mantissa/exponent are zero
        EXPECT_EQ(half_pos_zero, 0x0000U);
        EXPECT_EQ(half_neg_zero, 0x8000U);

        // Round-trip back to float32
        const auto float_pos_back = half_to_float(half_pos_zero);
        const auto float_neg_back = half_to_float(half_neg_zero);

        EXPECT_EQ(bit_cast<uint32_t>(float_pos_back), 0x00000000U);
        EXPECT_EQ(bit_cast<uint32_t>(float_neg_back), 0x80000000U);
    }

    // =========================================================================
    // SECTION: Infinity Representation Tests
    // =========================================================================

    /// @brief Verifies positive and negative infinity conversion and round-tripping.
    TEST(half_tests, half_infinity_representation)
    {
        // 1. Setup: Positive and negative infinity float32 bit representations
        const auto pos_inf_f = bit_cast<float>(0x7F800000U);
        const auto neg_inf_f = bit_cast<float>(0xFF800000U);

        // 2. Act: Convert float infinities to half precision
        const auto half_pos_inf = float_to_half(pos_inf_f);
        const auto half_neg_inf = float_to_half(neg_inf_f);

        // 3. Assert: Exponent bits are all 1s and mantissa is 0
        EXPECT_EQ(half_pos_inf, 0x7C00U);
        EXPECT_EQ(half_neg_inf, 0xFC00U);
        EXPECT_TRUE(is_inf_half(half_pos_inf));
        EXPECT_TRUE(is_inf_half(half_neg_inf));
        EXPECT_FALSE(is_nan_half(half_pos_inf));
        EXPECT_FALSE(is_nan_half(half_neg_inf));

        // Reconvert to float32
        const auto float_pos_back = half_to_float(half_pos_inf);
        const auto float_neg_back = half_to_float(half_neg_inf);

        EXPECT_EQ(bit_cast<uint32_t>(float_pos_back), 0x7F800000U);
        EXPECT_EQ(bit_cast<uint32_t>(float_neg_back), 0xFF800000U);
    }

    // =========================================================================
    // SECTION: Quiet and Signaling NaN Tests
    // =========================================================================

    /// @brief Verifies quiet and signaling NaN handling, payload preservation, and non-collapse to infinity.
    TEST(half_tests, half_nan_quiet_and_signaling)
    {
        // 1. Setup: Various quiet and signaling NaNs in single precision
        // Standard quiet NaN (quiet bit 22 set, payload zero)
        const auto standard_qnan = bit_cast<float>(0x7FC00000U);
        // Negative quiet NaN
        const auto neg_qnan = bit_cast<float>(0xFFC00000U);
        // Quiet NaN with arbitrary payload bits in upper positions
        const auto payload_qnan = bit_cast<float>(0x7FEA0000U);
        // Signaling NaN with upper payload bit set (quiet bit 22 is 0)
        const auto upper_snan = bit_cast<float>(0x7FA00000U);
        // Signaling NaN with ONLY the lowest bit set (all bits [22:13] are zero)
        const auto lowest_bit_snan = bit_cast<float>(0x7F800001U);

        // 2. Act: Convert to half precision
        const auto half_std_qnan = float_to_half(standard_qnan);
        const auto half_neg_qnan = float_to_half(neg_qnan);
        const auto half_payload_qnan = float_to_half(payload_qnan);
        const auto half_upper_snan = float_to_half(upper_snan);
        const auto half_lowest_bit_snan = float_to_half(lowest_bit_snan);

        // 3. Assert: All results are NaN, non-zero payload is preserved, and NaN never collapses to infinity
        EXPECT_TRUE(is_nan_half(half_std_qnan));
        EXPECT_EQ(half_std_qnan, 0x7E00U); // Quiet bit 9 set, payload 0, never clobbered by | 1U

        EXPECT_TRUE(is_nan_half(half_neg_qnan));
        EXPECT_EQ(half_neg_qnan, 0xFE00U);

        EXPECT_TRUE(is_nan_half(half_payload_qnan));
        EXPECT_NE(half_payload_qnan & tempest::math::float16_mantissa_mask, 0U);

        EXPECT_TRUE(is_nan_half(half_upper_snan));
        EXPECT_NE(half_upper_snan & tempest::math::float16_mantissa_mask, 0U);

        // Crucial bug test: lowest_bit_snan must NOT collapse to infinity (0x7C00)
        EXPECT_TRUE(is_nan_half(half_lowest_bit_snan));
        EXPECT_NE(half_lowest_bit_snan, 0x7C00U);
        EXPECT_NE(half_lowest_bit_snan & tempest::math::float16_mantissa_mask, 0U);

        // Reconvert half NaNs to float and ensure isnan is true
        EXPECT_TRUE(tempest::isnan(half_to_float(half_std_qnan)));
        EXPECT_TRUE(tempest::isnan(half_to_float(half_neg_qnan)));
        EXPECT_TRUE(tempest::isnan(half_to_float(half_payload_qnan)));
        EXPECT_TRUE(tempest::isnan(half_to_float(half_upper_snan)));
        EXPECT_TRUE(tempest::isnan(half_to_float(half_lowest_bit_snan)));
    }

    // =========================================================================
    // SECTION: Subnormal and Underflow Rounding Tests
    // =========================================================================

    /// @brief Verifies subnormal representations, exact decoding, and round-to-nearest ties-to-even behavior.
    TEST(half_tests, half_subnormals_and_underflow)
    {
        // 1. Setup: Smallest and largest representable half subnormals
        // Smallest subnormal: 2^-24 = 5.9604644775390625e-8
        constexpr auto half_min_subnormal = 0x0001U;
        // Largest subnormal: 1023 * 2^-24 = (1 - 2^-10) * 2^-14
        constexpr auto half_max_subnormal = 0x03FFU;

        // 2. Act & Assert: Exact round-trip of half subnormal extremes
        const auto float_min_sub = half_to_float(half_min_subnormal);
        const auto float_max_sub = half_to_float(half_max_subnormal);

        EXPECT_EQ(float_to_half(float_min_sub), half_min_subnormal);
        EXPECT_EQ(float_to_half(float_max_sub), half_max_subnormal);

        // 3. Round-up test for values in (2^-25, 2^-24) with unbiased_exponent == -25:
        // A float value like 1.5 * 2^-25 is > 2^-25, so it MUST round UP to 2^-24 (0x0001)!
        // Float32 encoding for 1.5 * 2^-25:
        // Exponent = 127 - 25 = 102 (0x66), mantissa = 0x00400000
        const auto float_one_and_half_2_pow_minus_25 = bit_cast<float>((102U << 23) | 0x00400000U);
        EXPECT_EQ(float_to_half(float_one_and_half_2_pow_minus_25), 0x0001U);

        // Value with mantissa = 1 at exponent -25 (strictly > 2^-25) must also round up to 0x0001
        const auto float_just_above_midpoint = bit_cast<float>((102U << 23) | 0x00000001U);
        EXPECT_EQ(float_to_half(float_just_above_midpoint), 0x0001U);

        // Exact midpoint 1.0 * 2^-25:
        // Tie between 0x0000 (even) and 0x0001 (odd). Round-to-nearest, ties-to-even MUST round down to 0x0000!
        const auto float_exact_midpoint = bit_cast<float>(102U << 23);
        EXPECT_EQ(float_to_half(float_exact_midpoint), 0x0000U);

        // Negative counterpart: -1.5 * 2^-25 must round up to -2^-24 (0x8001)
        const auto float_neg_one_and_half = bit_cast<float>(0x80000000U | (102U << 23) | 0x00400000U);
        EXPECT_EQ(float_to_half(float_neg_one_and_half), 0x8001U);

        // Float32 denormals (< 2^-126) flush to zero with preserved sign
        const auto float_denormal_pos = bit_cast<float>(0x00000001U);
        const auto float_denormal_neg = bit_cast<float>(0x80000001U);
        EXPECT_EQ(float_to_half(float_denormal_pos), 0x0000U);
        EXPECT_EQ(float_to_half(float_denormal_neg), 0x8000U);
    }

    // =========================================================================
    // SECTION: Round-to-Nearest, Ties-to-Even Tests
    // =========================================================================

    /// @brief Verifies strict round-to-nearest, ties-to-even behavior across normal and subnormal ranges.
    TEST(half_tests, half_round_to_nearest_ties_to_even)
    {
        // 1. Boundary between largest subnormal and smallest normal:
        // Largest subnormal: 1023 * 2^-24
        // Smallest normal:   1024 * 2^-24 = 2^-14
        // Exact midpoint:    1023.5 * 2^-24
        // 1023 is odd, 1024 is even. Under ties-to-even, 1023.5 must round to 1024 (0x0400)!
        // In float32: unbiased exponent is -15, float exponent is 112 (0x70).
        // 1023.5 * 2^-24 = (1 + 1023/1024) * 2^-15 with round bit set:
        // float significand has bits [22:13] = 0x3FF, bit 12 = 1, bits [11:0] = 0.
        const auto subnormal_to_normal_tie = bit_cast<float>((112U << 23) | (0x3FFU << 13) | (1U << 12));
        EXPECT_EQ(float_to_half(subnormal_to_normal_tie), 0x0400U);

        // 2. Normal range tie: 1.0f (half 0x3C00, even mantissa 0)
        // In half, at exponent 0 (biased 15), 1 ULP is 2^-10 = 1/1024.
        // Half ULP is 2^-11 = 1/2048.
        // Float 1.0f + 2^-11 is an exact tie between 0x3C00 (even) and 0x3C01 (odd).
        // Ties-to-even must round to 0x3C00 (even)!
        const auto normal_tie_even = 1.0f + (1.0f / 2048.0f);
        EXPECT_EQ(float_to_half(normal_tie_even), 0x3C00U);

        // Float 1.0f + 2^-11 + epsilon is strictly greater than midpoint: must round UP to 0x3C01
        const auto normal_above_tie = bit_cast<float>(bit_cast<uint32_t>(normal_tie_even) + 1U);
        EXPECT_EQ(float_to_half(normal_above_tie), 0x3C01U);

        // Float 1.0f + 2^-10 + 2^-11 is a tie between 0x3C01 (odd) and 0x3C02 (even).
        // Ties-to-even must round UP to 0x3C02 (even)!
        const auto normal_tie_odd = 1.0f + (1.0f / 1024.0f) + (1.0f / 2048.0f);
        EXPECT_EQ(float_to_half(normal_tie_odd), 0x3C02U);
    }

    // =========================================================================
    // SECTION: Max Normal and Overflow to Infinity Tests
    // =========================================================================

    /// @brief Verifies maximum finite normal values and IEEE-754 overflow boundary at 65520.0f.
    TEST(half_tests, half_max_normal_and_overflow)
    {
        // 1. Setup: Maximum finite half is 65504.0f (exponent 30, mantissa 1023 -> 0x7BFF)
        constexpr auto max_half = 65504.0f;
        EXPECT_EQ(float_to_half(max_half), 0x7BFFU);
        EXPECT_EQ(float_to_half(-max_half), 0xFBFFU);
        EXPECT_FLOAT_EQ(half_to_float(0x7BFFU), max_half);
        EXPECT_FLOAT_EQ(half_to_float(0xFBFFU), -max_half);

        // 2. Act: Values between 65504.0f and 65520.0f
        // 65519.0f is strictly below the halfway mark 65520.0f -> rounds down to 65504.0f
        EXPECT_EQ(float_to_half(65519.0f), 0x7BFFU);
        EXPECT_EQ(float_to_half(-65519.0f), 0xFBFFU);

        // 65520.0f is the exact tie between 65504 (mantissa ends in 1, odd) and 65536 (infinity).
        // Under ties-to-even, it must round UP to infinity (0x7C00)!
        EXPECT_EQ(float_to_half(65520.0f), 0x7C00U);
        EXPECT_EQ(float_to_half(-65520.0f), 0xFC00U);

        // Values above 65520.0f overflow to infinity
        EXPECT_EQ(float_to_half(65536.0f), 0x7C00U);
        EXPECT_EQ(float_to_half(100000.0f), 0x7C00U);
        EXPECT_EQ(float_to_half(1e30f), 0x7C00U);
        EXPECT_EQ(float_to_half(-1e30f), 0xFC00U);
    }

    // =========================================================================
    // SECTION: Exhaustive 65,536 Half-Precision Round-Trip Sweep
    // =========================================================================

    /// @brief Exhaustively sweeps all 65,536 uint16_t half-float bit patterns to verify round-trip fidelity.
    TEST(half_tests, half_exhaustive_65536_roundtrip)
    {
        for (uint32_t pattern = 0; pattern <= 0xFFFFU; ++pattern)
        {
            const auto half_input = static_cast<uint16_t>(pattern);
            const auto float_val = half_to_float(half_input);

            if (is_nan_half(half_input))
            {
                // Must produce a float NaN
                EXPECT_TRUE(tempest::isnan(float_val));

                // Converting back must reproduce a half NaN
                const auto half_back = float_to_half(float_val);
                EXPECT_TRUE(is_nan_half(half_back));

                // Quiet vs signaling status (bit 9) must be preserved
                EXPECT_EQ(half_input & tempest::math::float16_quiet_nan_mask,
                          half_back & tempest::math::float16_quiet_nan_mask);
            }
            else
            {
                // All finite numbers (+/- 0, subnormals, normals) and infinities MUST round-trip bit-exact
                const auto half_back = float_to_half(float_val);
                ASSERT_EQ(half_back, half_input);
            }
        }
    }
