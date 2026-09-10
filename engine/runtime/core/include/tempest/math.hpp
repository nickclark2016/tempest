#ifndef tempest_core_math_hpp
#define tempest_core_math_hpp

#include <tempest/concepts.hpp>
#include <tempest/limits.hpp>

namespace tempest
{
    constexpr auto isnan(floating_point auto x) noexcept -> bool
    {
        return x != x;
    }

    constexpr auto isinf(floating_point auto x) noexcept -> bool
    {
        return x == numeric_limits<decltype(x)>::infinity();
    }

    constexpr auto isfinite(floating_point auto x) noexcept -> bool
    {
        return !isnan(x) && !isinf(x);
    }

    constexpr auto signbit(floating_point auto x) noexcept -> bool
    {
        return x < 0;
    }

    constexpr auto signbit(integral auto x) noexcept
    {
        return x < 0;
    }

    constexpr auto abs(floating_point auto x) noexcept
    {
        return x < 0 ? -x : x;
    }

    constexpr auto abs(integral auto x) noexcept
    {
        return x < 0 ? -x : x;
    }
} // namespace tempest

#endif // tempest_core_math_hpp
