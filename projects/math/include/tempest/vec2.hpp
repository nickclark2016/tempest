#ifndef tempest_math_vec2_hpp__
#define tempest_math_vec2_hpp__

#include "math_utils.hpp"

#include <cstddef>
#include <type_traits>

namespace tempest::math
{
    template <typename T>
    struct alignas(sizeof(T) * 2) vec2
    {
#if defined(_MSC_VER) && !defined(__clang__)
#pragma warning(push)
#pragma warning(disable: 4201)
#endif
        union {
            T data[2];
            struct
            {
                T r;
                T g;
            };
            struct
            {
                T x;
                T y;
            };
            struct
            {
                T u;
                T v;
            };
        };
#if defined(_MSC_VER) && !defined(__clang__)
#pragma warning(pop)
#endif

        constexpr vec2() = default;
        constexpr vec2(const T scalar);
        constexpr vec2(const T x, const T y);

        constexpr T& operator[](const std::size_t index) noexcept;
        constexpr const T& operator[](const std::size_t index) const noexcept;

        vec2& operator+=(const vec2& rhs) noexcept;
        vec2& operator-=(const vec2& rhs) noexcept;
        vec2& operator*=(const vec2& rhs) noexcept;
        vec2& operator/=(const vec2& rhs) noexcept;
    };

    template <typename T>
    vec2(const T) -> vec2<T>;

    template <typename T>
    vec2(const T, const T, const T, const T) -> vec2<T>;

    template <typename T>
    vec2(const vec2<T>&) -> vec2<T>;

    template <typename T>
    vec2(vec2<T>&&) -> vec2<T>;

    // Implementation

    template <typename T>
    inline constexpr vec2<T>::vec2(const T scalar) : vec2(scalar, scalar)
    {
    }

    template <typename T>
    inline constexpr vec2<T>::vec2(const T x, const T y) : x(x), y(y)
    {
    }

    template <typename T>
    inline constexpr T& vec2<T>::operator[](const std::size_t index) noexcept
    {
        return data[index];
    }

    template <typename T>
    inline constexpr const T& vec2<T>::operator[](const std::size_t index) const noexcept
    {
        return data[index];
    }

    template <typename T>
    inline vec2<T>& vec2<T>::operator+=(const vec2& rhs) noexcept
    {
        x += rhs.x;
        y += rhs.y;
        return *this;
    }

    template <typename T>
    inline vec2<T>& vec2<T>::operator-=(const vec2& rhs) noexcept
    {
        x -= rhs.x;
        y -= rhs.y;
        return *this;
    }

    template <typename T>
    inline vec2<T>& vec2<T>::operator*=(const vec2& rhs) noexcept
    {
        x *= rhs.x;
        y *= rhs.y;
        return *this;
    }

    template <typename T>
    inline vec2<T>& vec2<T>::operator/=(const vec2& rhs) noexcept
    {
        x /= rhs.x;
        y /= rhs.y;
        return *this;
    }

    template <typename T>
    inline bool operator==(const vec2<T>& lhs, const vec2<T>& rhs) noexcept
    {
        return lhs[0] == rhs[0] && lhs[1] == rhs[1];
    }

    template <typename T>
    inline constexpr bool operator!=(const vec2<T>& lhs, const vec2<T>& rhs) noexcept
    {
        return lhs[0] != rhs[0] || lhs[1] != rhs[1];
    }

    template <typename T>
    inline constexpr vec2<T> operator+(const vec2<T>& lhs, const vec2<T>& rhs) noexcept
    {
        const vec2<T> result = {lhs[0] + rhs[0], lhs[1] + rhs[1]};
        return result;
    }

    template <typename T>
    inline constexpr vec2<T> operator-(const vec2<T>& lhs, const vec2<T>& rhs) noexcept
    {
        const vec2<T> result = {lhs[0] - rhs[0], lhs[1] - rhs[1]};
        return result;
    }

    template <typename T>
    inline constexpr vec2<T> operator*(const vec2<T>& lhs, const vec2<T>& rhs) noexcept
    {
        const vec2<T> result = {lhs[0] * rhs[0], lhs[1] * rhs[1]};
        return result;
    }

    template <typename T>
    inline constexpr vec2<T> operator*(const T scalar, const vec2<T>& rhs) noexcept
    {
        const vec2<T> result = {scalar * rhs[0], scalar * rhs[1]};
        return result;
    }

    template <typename T>
    inline constexpr vec2<T> operator/(const vec2<T>& lhs, const vec2<T>& rhs) noexcept
    {
        const vec2<T> result = {lhs[0] / rhs[0], lhs[1] / rhs[1]};
        return result;
    }

    template <typename T>
    inline constexpr vec2<T> operator/(const vec2<T>& lhs, const T scalar) noexcept
    {
        const vec2<T> result = {lhs[0] / scalar, lhs[1] / scalar};
        return result;
    }

    template <typename T>
    inline constexpr T norm(const vec2<T>& v)
    {
        return std::sqrt(v.x * v.x + v.y * v.y);
    }

    template <typename T>
    inline constexpr vec2<T> normalize(const vec2<T>& v)
    {
        return v / norm(v);
    }
} // namespace tempest::math

#endif // tempest_math_vec2_hpp__
