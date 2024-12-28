#ifndef tempest_math_math_utils_hpp__
#define tempest_math_math_utils_hpp__

#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numeric>
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

    template <typename T>
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
