#ifndef tempest_math_math_utils_hpp__
#define tempest_math_math_utils_hpp__

#include <bit>
#include <cmath>
#include <cstdint>
#include <type_traits>

namespace tempest::math
{
    namespace constants
    {
        template <typename T>
        constexpr T pi = static_cast<T>(3.14159265359);

        template <typename T>
        constexpr T half_pi = pi<T> / static_cast<T>(2);

        template <typename T>
        constexpr T inv_pi = static_cast<T>(1) / pi<T>;
    }; // namespace constants

    namespace detail
    {
        inline constexpr std::uint32_t as_u32_t_bits(const float v)
        {
            return std::bit_cast<std::uint32_t>(v);
        }

        inline constexpr float as_float_bits(const std::uint32_t v)
        {
            return std::bit_cast<float>(v);
        }
    } // namespace detail

    template <typename T>
    inline constexpr T fast_inv_sqrt(T value) noexcept
    {
        if constexpr (std::is_same_v<T, float> && std::numeric_limits<T>::is_iec559)
        {
            // Quake 3 Fast Inversion Square Root
            const float y = std::bit_cast<float>(0x5f3759df - (detail::as_u32_t_bits(value) >> 1));
            return y * (1.5f - 0.5f * value * y * y);
        }
        return static_cast<T>(std::sqrt(value));
    }

    template <typename T>
    inline constexpr T clamp(T value, T lower, T upper)
    {
        const T u = static_cast<T>(std::min(value, upper));
        return static_cast<T>(std::max(lower, u));
    }

    template <typename T>
    inline constexpr T as_radians(const T degrees) noexcept
    {
        const T half_circle = static_cast<T>(180);
        return degrees / half_circle * constants::pi<T>;
    }

    template <typename T>
    inline constexpr T as_degrees(const T radians) noexcept
    {
        const T pi_rad = radians * constants::inv_pi<T>;
        return pi_rad * static_cast<T>(180);
    }

    inline std::uint16_t compress_to_half(const float value)
    {
        return static_cast<std::uint16_t>(std::round(clamp(value, -1.0f, +1.0f) * 32767.0f));
    }

    inline constexpr float inflate_to_float(const std::uint16_t value)
    {
        // IEEE-754 16-bit floating-point format (without infinity): 1-5-10, exp-15, +-131008.0, +-6.1035156E-5,
        // +-5.9604645E-8, 3.311 digits
        const std::uint32_t e = (value & 0x7C00) >> 10; // exponent
        const std::uint32_t m = (value & 0x03FF) << 13; // mantissa
        const std::uint32_t v = detail::as_u32_t_bits(static_cast<float>(m)) >>
                                23; // evil log2 bit hack to count leading zeros in denormalized format
        return detail::as_float_bits(
            (value & 0x8000) << 16 | (e != 0) * ((e + 112) << 23 | m) |
            ((e == 0) & (m != 0)) *
                ((v - 37) << 23 | ((m << (150 - v)) & 0x007FE000))); // sign : normalized : denormalized
    }

    template <typename T>
    inline constexpr T inverse_lerp(const T value, const T low, const T high)
    {
        return (value - low) / (high - low);
    }

    template <typename T>
    inline constexpr T lerp(const T low, const T high, const T t)
    {
        return low + t * (high - low);
    }

    template <typename T>
    inline constexpr T reproject(const T value, const T old_min, const T old_max, const T new_min = static_cast<T>(-1),
                                 const T new_max = static_cast<T>(1))
    {
        const auto t = inverse_lerp(value, old_min, old_max);
        return lerp(new_min, new_max, t);
    }

    template <std::integral T>
    inline constexpr T div_ceil(T x, T y)
    {
        if (x != 0)
        {
            return 1 + ((x - 1) / y);
        }
        return 0;
    }
} // namespace tempest::math

#endif // tempest_math_math_utils_hpp__
