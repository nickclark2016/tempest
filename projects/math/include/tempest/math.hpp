#ifndef tempest_math_hpp
#define tempest_math_hpp

#include <limits>
#include <numbers>

#include <tempest/vec.hpp>

namespace tempest::math
{
    namespace constants
    {
        template <std::floating_point T> inline constexpr T pi = std::numbers::pi_v<T>;
        template <std::floating_point T> inline constexpr T two_pi = pi<T> * static_cast<T>(2);
        template <std::floating_point T> inline constexpr T half_pi = pi<T> / static_cast<T>(2);

        template <std::floating_point T> inline constexpr T max = std::numeric_limits<T>::max();
        template <std::floating_point T> inline constexpr T epsilon = std::numeric_limits<T>::epsilon();
        template <std::floating_point T> inline constexpr T infinity = std::numeric_limits<T>::infinity();

        template <typename T> concept floating_point_ieee754 = std::numeric_limits<T>::is_iec559;
        template <floating_point_ieee754 T> inline constexpr T negative_infinity = -infinity<T>;

        template <std::floating_point T> inline constexpr T degrees_to_radians = pi<T> / static_cast<T>(180);
        template <std::floating_point T> inline constexpr T radians_to_degrees = static_cast<T>(180) / pi<T>;
    } // namespace constants
} // namespace tempest

#endif // tempest_math_hpp