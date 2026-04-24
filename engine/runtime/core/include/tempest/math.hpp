#ifndef tempest_core_math_hpp
#define tempest_core_math_hpp

#include <tempest/concepts.hpp>
#include <tempest/limits.hpp>

namespace tempest
{
    inline constexpr bool isnan(floating_point auto x) noexcept
    {
        return x != x;
    }

    inline constexpr bool isinf(floating_point auto x) noexcept
    {
        return x == numeric_limits<decltype(x)>::infinity();
    }

    inline constexpr bool isfinite(floating_point auto x) noexcept
    {
        return !isnan(x) && !isinf(x);
    }

    inline constexpr bool signbit(floating_point auto x) noexcept
    {
        return x < 0;
    }

    inline constexpr auto signbit(integral auto x) noexcept
    {
        return x < 0;
    }

    inline constexpr auto abs(floating_point auto x) noexcept
    {
        return x < 0 ? -x : x;
    }

    inline constexpr auto abs(integral auto x) noexcept
    {
        return x < 0 ? -x : x;
    }
} // namespace tempest

#endif // tempest_core_math_hpp
