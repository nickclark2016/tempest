#ifndef tempest_math_vec2_hpp__
#define tempest_math_vec2_hpp__

#include <cstddef>
#include <type_traits>

namespace tempest::math
{
    template <typename T> struct alignas(sizeof(T) * 2) vec2
    {
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

        constexpr vec2();
        constexpr vec2(const T scalar);
        constexpr vec2(const T x, const T y);

        constexpr T& operator[](const std::size_t index) noexcept;
        constexpr const T& operator[](const std::size_t index) const noexcept;

        vec2& operator+=(const vec2& rhs) noexcept;
        vec2& operator-=(const vec2& rhs) noexcept;
        vec2& operator*=(const vec2& rhs) noexcept;
        vec2& operator/=(const vec2& rhs) noexcept;
    };

    template <typename T> vec2(const T) -> vec2<T>;

    template <typename T> vec2(const T, const T, const T, const T) -> vec2<T>;

    template <typename T> vec2(const vec2<T>&) -> vec2<T>;

    template <typename T> vec2(vec2<T>&&) -> vec2<T>;

    // Implementation

    template <typename T> inline constexpr vec2<T>::vec2() : vec2(T())
    {
    }

    template <typename T> inline constexpr vec2<T>::vec2(const T scalar) : vec2(scalar, scalar)
    {
    }

    template <typename T> inline constexpr vec2<T>::vec2(const T x, const T y) : x(x), y(y)
    {
    }

    template <typename T> inline constexpr T& vec2<T>::operator[](const std::size_t index) noexcept
    {
        return data[index];
    }

    template <typename T> inline constexpr const T& vec2<T>::operator[](const std::size_t index) const noexcept
    {
        return data[index];
    }

    template <typename T> inline vec2<T>& vec2<T>::operator+=(const vec2& rhs) noexcept
    {
        x += rhs.x;
        y += rhs.y;
        return *this;
    }

    template <typename T> inline vec2<T>& vec2<T>::operator-=(const vec2& rhs) noexcept
    {
        x -= rhs.x;
        y -= rhs.y;
        return *this;
    }

    template <typename T> inline vec2<T>& vec2<T>::operator*=(const vec2& rhs) noexcept
    {
        x *= rhs.x;
        y *= rhs.y;
        return *this;
    }

    template <typename T> inline vec2<T>& vec2<T>::operator/=(const vec2& rhs) noexcept
    {
        x /= rhs.x;
        y /= rhs.y;
        return *this;
    }

    template <typename T> inline bool operator==(const vec2<T>& lhs, const vec2<T>& rhs) noexcept
    {
        return lhs[0] == rhs[0] && lhs[1] == rhs[1];
    }

    template <typename T> inline constexpr bool operator!=(const vec2<T>& lhs, const vec2<T>& rhs) noexcept
    {
        return lhs[0] != rhs[0] || lhs[1] != rhs[1];
    }

    template <typename T> inline constexpr vec2<T> operator+(const vec2<T>& lhs, const vec2<T>& rhs) noexcept
    {
        const vec2<T> result = {lhs[0] + rhs[0], lhs[1] + rhs[1]};
        return result;
    }

    template <typename T> inline constexpr vec2<T> operator-(const vec2<T>& lhs, const vec2<T>& rhs) noexcept
    {
        const vec2<T> result = {lhs[0] - rhs[0], lhs[1] - rhs[1]};
        return result;
    }

    template <typename T> inline constexpr vec2<T> operator*(const vec2<T>& lhs, const vec2<T>& rhs) noexcept
    {
        const vec2<T> result = {lhs[0] * rhs[0], lhs[1] * rhs[1]};
        return result;
    }

    template <typename T> inline constexpr vec2<T> operator*(const T scalar, const vec2<T>& rhs) noexcept
    {
        const vec2<T> result = {scalar * rhs[0], scalar * rhs[1]};
        return result;
    }

    template <typename T> inline constexpr vec2<T> operator/(const vec2<T>& lhs, const vec2<T>& rhs) noexcept
    {
        const vec2<T> result = {lhs[0] / rhs[0], lhs[1] / rhs[1]};
        return result;
    }
} // namespace tempest::math

#endif // tempest_math_vec2_hpp__
