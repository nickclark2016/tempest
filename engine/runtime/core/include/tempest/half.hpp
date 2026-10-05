#ifndef tempest_core_half_hpp
#define tempest_core_half_hpp

#include <tempest/bit.hpp>
#include <tempest/int.hpp>

namespace tempest::math
{
    // =========================================================================
    // IEEE-754 Single-Precision (binary32) Parameters
    // =========================================================================
    constexpr uint32_t float32_sign_mask = 0x8000'0000U;
    constexpr uint32_t float32_exponent_mask = 0x7F80'0000U;
    constexpr uint32_t float32_mantissa_mask = 0x007F'FFFFU;
    constexpr uint32_t float32_implicit_leading_one = 0x0080'0000U;
    constexpr uint32_t float32_quiet_nan_mask = 0x0040'0000U;
    constexpr uint32_t float32_sign_shift = 31U;
    constexpr uint32_t float32_exponent_shift = 23U;
    constexpr uint32_t float32_mantissa_bits = 23U;
    constexpr int32_t float32_exponent_bias = 127;
    constexpr uint32_t float32_max_exponent_field = 0xFFU;

    // =========================================================================
    // IEEE-754 Half-Precision (binary16) Parameters
    // =========================================================================
    constexpr uint16_t float16_sign_mask = 0x8000U;
    constexpr uint16_t float16_exponent_mask = 0x7C00U;
    constexpr uint16_t float16_mantissa_mask = 0x03FFU;
    constexpr uint16_t float16_implicit_leading_one = 0x0400U;
    constexpr uint16_t float16_quiet_nan_mask = 0x0200U;
    constexpr uint16_t float16_infinity_bits = 0x7C00U;
    constexpr uint32_t float16_sign_shift = 15U;
    constexpr uint32_t float16_exponent_shift = 10U;
    constexpr uint32_t float16_mantissa_bits = 10U;
    constexpr int32_t float16_exponent_bias = 15;
    constexpr uint32_t float16_max_exponent_field = 0x1FU;
    constexpr int32_t float16_max_unbiased_exponent = 15;
    constexpr int32_t float16_min_normal_unbiased_exponent = -14;
    constexpr int32_t float16_min_subnormal_unbiased_exponent = -24;

    // =========================================================================
    // Conversion Shift and Bias Deltas
    // =========================================================================
    constexpr uint32_t float_to_half_mantissa_shift = float32_mantissa_bits - float16_mantissa_bits; // 13
    constexpr uint32_t float_to_half_sign_shift = float32_sign_shift - float16_sign_shift;         // 16
    constexpr int32_t exponent_bias_delta = float32_exponent_bias - float16_exponent_bias;          // 112
    constexpr uint32_t max_subnormal_shift = float32_mantissa_bits + 1U;                            // 24 (corresponds to 2^-25)

    /// @brief Checks if a half-precision 16-bit float represents NaN.
    [[nodiscard]] constexpr auto is_nan_half(uint16_t half_val) noexcept -> bool
    {
        return (half_val & float16_exponent_mask) == float16_infinity_bits &&
               (half_val & float16_mantissa_mask) != 0;
    }

    /// @brief Checks if a half-precision 16-bit float represents positive or negative infinity.
    [[nodiscard]] constexpr auto is_inf_half(uint16_t half_val) noexcept -> bool
    {
        return (half_val & (float16_exponent_mask | float16_mantissa_mask)) == float16_infinity_bits;
    }

    /// @brief Converts an IEEE-754 32-bit single-precision float to a 16-bit half-precision float
    /// using IEEE-754 round-to-nearest, ties-to-even rounding mode.
    ///
    /// Preserves +0.0f, -0.0f, +Infinity, -Infinity, quiet/signaling NaN payload states,
    /// subnormal rounding without premature truncation, and exact tie-breaking.
    ///
    /// @param float_val IEEE-754 32-bit single precision value.
    /// @return 16-bit IEEE-754 half precision bit pattern.
    [[nodiscard]] constexpr auto float_to_half(float float_val) noexcept -> uint16_t
    {
        const auto float_bits = bit_cast<uint32_t>(float_val);
        const auto sign_bit = static_cast<uint16_t>((float_bits & float32_sign_mask) >> float_to_half_sign_shift);
        const auto raw_exponent = static_cast<uint32_t>((float_bits & float32_exponent_mask) >> float32_exponent_shift);
        const auto mantissa = float_bits & float32_mantissa_mask;

        // 1. Zero and Subnormal Float32 values
        // Single-precision subnormals (< 2^-126) strictly underflow below half precision's minimum subnormal
        // round-up threshold (2^-25) and flush to zero with preserved sign.
        if (raw_exponent == 0)
        {
            return sign_bit;
        }

        // 2. Infinity and NaN
        if (raw_exponent == float32_max_exponent_field)
        {
            if (mantissa != 0)
            {
                // NaN: preserve upper payload bits including the quiet bit (bit 22 -> bit 9)
                auto half_payload = static_cast<uint16_t>(mantissa >> float_to_half_mantissa_shift);
                if (half_payload == 0)
                {
                    // If all mantissa bits were below bit 13, ensure the payload does not collapse to Infinity
                    half_payload = 1;
                }
                return sign_bit | float16_infinity_bits | half_payload;
            }
            return sign_bit | float16_infinity_bits;
        }

        const auto unbiased_exponent = static_cast<int32_t>(raw_exponent) - float32_exponent_bias;

        // 3. Overflow beyond maximum half-float exponent
        // If unbiased exponent > 15, value is >= 65536.0f, which strictly exceeds the half overflow threshold (65520.0f)
        if (unbiased_exponent > float16_max_unbiased_exponent)
        {
            return sign_bit | float16_infinity_bits;
        }

        // 4. Subnormal half-floats and underflow to zero
        if (unbiased_exponent < float16_min_normal_unbiased_exponent)
        {
            const auto full_significand = mantissa | float32_implicit_leading_one;
            const auto shift = static_cast<uint32_t>(-1 - unbiased_exponent);

            if (shift > max_subnormal_shift)
            {
                // Strictly below 2^-25; under round-to-nearest ties-to-even, flushes to zero
                return sign_bit;
            }

            const auto quotient = full_significand >> shift;
            const auto round_bit = (full_significand >> (shift - 1U)) & 1U;
            const auto sticky_mask = (1U << (shift - 1U)) - 1U;
            const auto sticky_bit = ((full_significand & sticky_mask) != 0) ? 1U : 0U;
            const auto lsb = quotient & 1U;

            // IEEE-754 round-to-nearest, ties-to-even
            const auto increment = round_bit & (lsb | sticky_bit);
            const auto rounded = quotient + increment;

            // If rounded == float16_implicit_leading_one (0x0400), it has carried into the smallest normal half
            return sign_bit | static_cast<uint16_t>(rounded);
        }

        // 5. Normal half-floats
        const auto full_significand = mantissa | float32_implicit_leading_one;
        constexpr auto shift = float_to_half_mantissa_shift;

        const auto quotient = full_significand >> shift;
        const auto round_bit = (full_significand >> (shift - 1U)) & 1U;
        constexpr auto sticky_mask = (1U << (shift - 1U)) - 1U;
        const auto sticky_bit = ((full_significand & sticky_mask) != 0) ? 1U : 0U;
        const auto lsb = quotient & 1U;

        // IEEE-754 round-to-nearest, ties-to-even
        const auto increment = round_bit & (lsb | sticky_bit);
        const auto rounded = quotient + increment;

        // Base exponent encoded at bit 10: (half_exponent - 1) << 10
        // Adding 'rounded' (which contains implicit 1 at bit 10) produces (half_exponent << 10) | half_mantissa.
        // If mantissa overflowed (rounded == 0x0800), bit 11 seamlessly increments half_exponent.
        const auto half_exponent = static_cast<uint32_t>(unbiased_exponent + float16_exponent_bias);
        const auto base = (half_exponent - 1U) << float16_exponent_shift;
        const auto result_bits = base + rounded;

        // Check if rounding caused an overflow to infinity (e.g. 65520.0f)
        if (result_bits >= (static_cast<uint32_t>(float16_max_exponent_field) << float16_exponent_shift))
        {
            return sign_bit | float16_infinity_bits;
        }

        return sign_bit | static_cast<uint16_t>(result_bits);
    }

    /// @brief Converts an IEEE-754 16-bit half-precision float to a 32-bit single-precision float.
    ///
    /// Preserves exact values for all 65,536 half-float representations: zeros, denormals/subnormals,
    /// normals, infinities, and NaNs (including quiet/signaling distinction).
    ///
    /// @param half_val 16-bit IEEE-754 half precision bit pattern.
    /// @return IEEE-754 32-bit single precision value.
    [[nodiscard]] constexpr auto half_to_float(uint16_t half_val) noexcept -> float
    {
        const auto sign = static_cast<uint32_t>(half_val & float16_sign_mask) << float_to_half_sign_shift;
        const auto half_exponent = static_cast<uint32_t>((half_val & float16_exponent_mask) >> float16_exponent_shift);
        const auto half_mantissa = static_cast<uint32_t>(half_val & float16_mantissa_mask);

        // 1. Zeros and Subnormals
        if (half_exponent == 0)
        {
            if (half_mantissa == 0)
            {
                return bit_cast<float>(sign);
            }

            // Subnormal: normalize mantissa using count-leading-zeros without a while-loop
            const auto clz = countl_zero(half_mantissa);
            // half_mantissa is in [1, 1023] (highest bit in [0, 9]), so clz in uint32_t is in [22, 31].
            // To position the leading 1 at float32 implicit bit 23: shift = 23 - (31 - clz) = clz - 8.
            const auto shift = static_cast<uint32_t>(clz - 8);
            const auto float_exponent = (static_cast<uint32_t>(exponent_bias_delta + 14) - shift) << float32_exponent_shift;
            const auto float_mantissa = (half_mantissa << shift) & float32_mantissa_mask;

            return bit_cast<float>(sign | float_exponent | float_mantissa);
        }

        // 2. Infinity and NaN
        if (half_exponent == float16_max_exponent_field)
        {
            const auto float_exponent = float32_max_exponent_field << float32_exponent_shift;
            const auto float_mantissa = half_mantissa << float_to_half_mantissa_shift;
            return bit_cast<float>(sign | float_exponent | float_mantissa);
        }

        // 3. Normal half numbers
        const auto float_exponent = (half_exponent + static_cast<uint32_t>(exponent_bias_delta)) << float32_exponent_shift;
        const auto float_mantissa = half_mantissa << float_to_half_mantissa_shift;

        return bit_cast<float>(sign | float_exponent | float_mantissa);
    }
} // namespace tempest::math

namespace tempest
{
    using math::float_to_half;
    using math::half_to_float;
    using math::is_inf_half;
    using math::is_nan_half;
} // namespace tempest

#endif // tempest_core_half_hpp
