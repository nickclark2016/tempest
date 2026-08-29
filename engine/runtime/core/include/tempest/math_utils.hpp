#ifndef tempest_math_math_utils_hpp__
#define tempest_math_math_utils_hpp__

#include <tempest/algorithm.hpp>
#include <tempest/api.hpp>
#include <tempest/int.hpp>
#include <tempest/limits.hpp>
#include <tempest/type_traits.hpp>

#if defined(_MSC_VER) && !defined(__clang__)
#include <math.h>
#endif

namespace tempest::math
{
    namespace constants
    {
        template <typename T>
        constexpr T pi = static_cast<T>(3.14159265358979323846);

        template <typename T>
        constexpr T half_pi = pi<T> / static_cast<T>(2);

        template <typename T>
        constexpr T inv_pi = static_cast<T>(1) / pi<T>;
    } // namespace constants

    inline auto sqrt(float x) noexcept -> float
    {
#if defined(__clang__) || defined(__GNUC__)
        return __builtin_sqrtf(x);
#else
        return ::sqrtf(x);
#endif
    }

    inline auto sqrt(double x) noexcept -> double
    {
#if defined(__clang__) || defined(__GNUC__)
        return __builtin_sqrt(x);
#else
        return ::sqrt(x);
#endif
    }

    inline auto cbrt(float x) noexcept -> float
    {
#if defined(__clang__) || defined(__GNUC__)
        return __builtin_cbrtf(x);
#else
        return ::cbrtf(x);
#endif
    }

    inline auto cbrt(double x) noexcept -> double
    {
#if defined(__clang__) || defined(__GNUC__)
        return __builtin_cbrt(x);
#else
        return ::cbrt(x);
#endif
    }

    inline auto sin(float x) noexcept -> float
    {
#if defined(__clang__) || defined(__GNUC__)
        return __builtin_sinf(x);
#else
        return ::sinf(x);
#endif
    }

    inline auto sin(double x) noexcept -> double
    {
#if defined(__clang__) || defined(__GNUC__)
        return __builtin_sin(x);
#else
        return ::sin(x);
#endif
    }

    inline auto cos(float x) noexcept -> float
    {
#if defined(__clang__) || defined(__GNUC__)
        return __builtin_cosf(x);
#else
        return ::cosf(x);
#endif
    }

    inline auto cos(double x) noexcept -> double
    {
#if defined(__clang__) || defined(__GNUC__)
        return __builtin_cos(x);
#else
        return ::cos(x);
#endif
    }

    inline auto tan(float x) noexcept -> float
    {
#if defined(__clang__) || defined(__GNUC__)
        return __builtin_tanf(x);
#else
        return ::tanf(x);
#endif
    }

    inline auto tan(double x) noexcept -> double
    {
#if defined(__clang__) || defined(__GNUC__)
        return __builtin_tan(x);
#else
        return ::tan(x);
#endif
    }

    inline auto asin(float x) noexcept -> float
    {
#if defined(__clang__) || defined(__GNUC__)
        return __builtin_asinf(x);
#else
        return ::asinf(x);
#endif
    }

    inline auto asin(double x) noexcept -> double
    {
#if defined(__clang__) || defined(__GNUC__)
        return __builtin_asin(x);
#else
        return ::asin(x);
#endif
    }

    inline auto acos(float x) noexcept -> float
    {
#if defined(__clang__) || defined(__GNUC__)
        return __builtin_acosf(x);
#else
        return ::acosf(x);
#endif
    }

    inline auto acos(double x) noexcept -> double
    {
#if defined(__clang__) || defined(__GNUC__)
        return __builtin_acos(x);
#else
        return ::acos(x);
#endif
    }

    inline auto atan(float x) noexcept -> float
    {
#if defined(__clang__) || defined(__GNUC__)
        return __builtin_atanf(x);
#else
        return ::atanf(x);
#endif
    }

    inline auto atan(double x) noexcept -> double
    {
#if defined(__clang__) || defined(__GNUC__)
        return __builtin_atan(x);
#else
        return ::atan(x);
#endif
    }

    inline auto atan2(float y, float x) noexcept -> float
    {
#if defined(__clang__) || defined(__GNUC__)
        return __builtin_atan2f(y, x);
#else
        return ::atan2f(y, x);
#endif
    }

    inline auto atan2(double y, double x) noexcept -> double
    {
#if defined(__clang__) || defined(__GNUC__)
        return __builtin_atan2(y, x);
#else
        return ::atan2(y, x);
#endif
    }

    inline auto floor(float x) noexcept -> float
    {
#if defined(__clang__) || defined(__GNUC__)
        return __builtin_floorf(x);
#else
        return ::floorf(x);
#endif
    }

    inline auto floor(double x) noexcept -> double
    {
#if defined(__clang__) || defined(__GNUC__)
        return __builtin_floor(x);
#else
        return ::floor(x);
#endif
    }

    inline auto ceil(float x) noexcept -> float
    {
#if defined(__clang__) || defined(__GNUC__)
        return __builtin_ceilf(x);
#else
        return ::ceilf(x);
#endif
    }

    inline auto ceil(double x) noexcept -> double
    {
#if defined(__clang__) || defined(__GNUC__)
        return __builtin_ceil(x);
#else
        return ::ceil(x);
#endif
    }

    inline auto round(float x) noexcept -> float
    {
#if defined(__clang__) || defined(__GNUC__)
        return __builtin_roundf(x);
#else
        return ::roundf(x);
#endif
    }

    inline auto round(double x) noexcept -> double
    {
#if defined(__clang__) || defined(__GNUC__)
        return __builtin_round(x);
#else
        return ::round(x);
#endif
    }

    inline auto trunc(float x) noexcept -> float
    {
#if defined(__clang__) || defined(__GNUC__)
        return __builtin_truncf(x);
#else
        return ::truncf(x);
#endif
    }

    inline auto trunc(double x) noexcept -> double
    {
#if defined(__clang__) || defined(__GNUC__)
        return __builtin_trunc(x);
#else
        return ::trunc(x);
#endif
    }

    inline auto fmod(float x, float y) noexcept -> float
    {
#if defined(__clang__) || defined(__GNUC__)
        return __builtin_fmodf(x, y);
#else
        return ::fmodf(x, y);
#endif
    }

    inline auto fmod(double x, double y) noexcept -> double
    {
#if defined(__clang__) || defined(__GNUC__)
        return __builtin_fmod(x, y);
#else
        return ::fmod(x, y);
#endif
    }

    inline auto pow(float x, float y) noexcept -> float
    {
#if defined(__clang__) || defined(__GNUC__)
        return __builtin_powf(x, y);
#else
        return ::powf(x, y);
#endif
    }

    inline auto pow(double x, double y) noexcept -> double
    {
#if defined(__clang__) || defined(__GNUC__)
        return __builtin_pow(x, y);
#else
        return ::pow(x, y);
#endif
    }

    inline auto exp(float x) noexcept -> float
    {
#if defined(__clang__) || defined(__GNUC__)
        return __builtin_expf(x);
#else
        return ::expf(x);
#endif
    }

    inline auto exp(double x) noexcept -> double
    {
#if defined(__clang__) || defined(__GNUC__)
        return __builtin_exp(x);
#else
        return ::exp(x);
#endif
    }

    inline auto log(float x) noexcept -> float
    {
#if defined(__clang__) || defined(__GNUC__)
        return __builtin_logf(x);
#else
        return ::logf(x);
#endif
    }

    inline auto log(double x) noexcept -> double
    {
#if defined(__clang__) || defined(__GNUC__)
        return __builtin_log(x);
#else
        return ::log(x);
#endif
    }

    inline auto log2(float x) noexcept -> float
    {
#if defined(__clang__) || defined(__GNUC__)
        return __builtin_log2f(x);
#else
        return ::log2f(x);
#endif
    }

    inline auto log2(double x) noexcept -> double
    {
#if defined(__clang__) || defined(__GNUC__)
        return __builtin_log2(x);
#else
        return ::log2(x);
#endif
    }

    inline auto log10(float x) noexcept -> float
    {
#if defined(__clang__) || defined(__GNUC__)
        return __builtin_log10f(x);
#else
        return ::log10f(x);
#endif
    }

    inline auto log10(double x) noexcept -> double
    {
#if defined(__clang__) || defined(__GNUC__)
        return __builtin_log10(x);
#else
        return ::log10(x);
#endif
    }

    using tempest::clamp;
    using tempest::max;
    using tempest::min;

    template <typename T>
    inline constexpr auto as_radians(const T degrees) noexcept -> T
    {
        constexpr auto half_circle = static_cast<T>(180);
        return degrees / half_circle * constants::pi<T>;
    }

    template <typename T>
    inline constexpr auto as_degrees(const T radians) noexcept -> T
    {
        constexpr auto pi_rad = constants::inv_pi<T>;
        return radians * pi_rad * static_cast<T>(180);
    }

    template <typename T>
    inline constexpr auto inverse_lerp(const T value, const T low, const T high) noexcept -> T
    {
        return (value - low) / (high - low);
    }

    template <typename T>
    inline constexpr auto lerp(const T low, const T high, const T t) noexcept -> T
    {
        return low + t * (high - low);
    }

    template <typename T>
    inline constexpr auto reproject(const T value, const T old_min, const T old_max,
                                    const T new_min = static_cast<T>(-1), const T new_max = static_cast<T>(1)) noexcept
        -> T
    {
        const auto t = inverse_lerp(value, old_min, old_max);
        return lerp(new_min, new_max, t);
    }

    template <typename T, typename U = T>
    inline constexpr auto div_ceil(T x, U y) noexcept -> T
    {
        if (x != 0)
        {
            return 1 + ((x - 1) / static_cast<T>(y));
        }
        return 0;
    }

    template <typename T, typename U = T>
    inline constexpr auto round_to_next_multiple(T x, U y) noexcept -> T
    {
        if (y == 0)
        {
            return x;
        }

        const auto remainder = x % y;
        if (remainder == 0)
        {
            return x;
        }

        return x + y - remainder;
    }

    inline auto pack_uint32x2(uint32_t x, uint32_t y) noexcept -> uint64_t
    {
        return (static_cast<uint64_t>(x) << 32) | static_cast<uint64_t>(y);
    }

    inline auto unpack_uint32x2(uint64_t packed, uint32_t& x, uint32_t& y) noexcept -> void
    {
        x = static_cast<uint32_t>(packed >> 32);
        y = static_cast<uint32_t>(packed & 0xFFFFFFFF);
    }
} // namespace tempest::math

#endif // tempest_math_math_utils_hpp__
