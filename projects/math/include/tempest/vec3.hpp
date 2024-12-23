#ifndef tempest_math_vec3_hpp__
#define tempest_math_vec3_hpp__

#include "math_utils.hpp"

#include <cstddef>
#include <type_traits>

namespace tempest::math
{
    template <typename T>
    struct alignas(sizeof(T) * 4) vec3
    {
        union {
            T data[4];
            struct
            {
                T r;
                T g;
                T b;
            };
            struct
            {
                T x;
                T y;
                T z;
            };
        };

        constexpr vec3();
        constexpr vec3(const T scalar);
        constexpr vec3(const T x, const T y, const T z);

        constexpr T& operator[](const std::size_t index) noexcept;
        constexpr const T& operator[](const std::size_t index) const noexcept;

        vec3& operator+=(const vec3& rhs) noexcept;
        vec3& operator-=(const vec3& rhs) noexcept;
        vec3& operator*=(const vec3& rhs) noexcept;
        vec3& operator/=(const vec3& rhs) noexcept;
    };

    template <typename T>
    vec3(const T) -> vec3<T>;

    template <typename T>
    vec3(const T, const T, const T, const T) -> vec3<T>;

    template <typename T>
    vec3(const vec3<T>&) -> vec3<T>;

    template <typename T>
    vec3(vec3<T>&&) -> vec3<T>;

    // Implementation

    template <typename T>
    inline constexpr vec3<T>::vec3() : vec3(T())
    {
    }

    template <typename T>
    inline constexpr vec3<T>::vec3(const T scalar) : vec3(scalar, scalar, scalar)
    {
    }

    template <typename T>
    inline constexpr vec3<T>::vec3(const T x, const T y, const T z) : x(x), y(y), z(z)
    {
    }

    template <typename T>
    inline constexpr T& vec3<T>::operator[](const std::size_t index) noexcept
    {
        return data[index];
    }

    template <typename T>
    inline constexpr const T& vec3<T>::operator[](const std::size_t index) const noexcept
    {
        return data[index];
    }

    template <typename T>
    inline vec3<T>& vec3<T>::operator+=(const vec3& rhs) noexcept
    {
        x += rhs.x;
        y += rhs.y;
        z += rhs.z;
        return *this;
    }

    template <typename T>
    inline vec3<T>& vec3<T>::operator-=(const vec3& rhs) noexcept
    {
        x -= rhs.x;
        y -= rhs.y;
        z -= rhs.z;
        return *this;
    }

    template <typename T>
    inline vec3<T>& vec3<T>::operator*=(const vec3& rhs) noexcept
    {
        x *= rhs.x;
        y *= rhs.y;
        z *= rhs.z;
        return *this;
    }

    template <typename T>
    inline vec3<T>& vec3<T>::operator/=(const vec3& rhs) noexcept
    {
        x /= rhs.x;
        y /= rhs.y;
        z /= rhs.z;
        return *this;
    }

    template <typename T>
    inline bool operator==(const vec3<T>& lhs, const vec3<T>& rhs) noexcept
    {
        return lhs[0] == rhs[0] && lhs[1] == rhs[1] && lhs[2] == rhs[2];
    }

    template <typename T>
    inline constexpr bool operator!=(const vec3<T>& lhs, const vec3<T>& rhs) noexcept
    {
        return lhs[0] != rhs[0] || lhs[1] != rhs[1] || lhs[2] != rhs[2];
    }

    template <typename T>
    inline constexpr vec3<T> operator+(const vec3<T>& lhs, const vec3<T>& rhs) noexcept
    {
        const vec3<T> result = {lhs[0] + rhs[0], lhs[1] + rhs[1], lhs[2] + rhs[2]};
        return result;
    }

    template <typename T>
    inline constexpr vec3<T> operator-(const vec3<T>& lhs, const vec3<T>& rhs) noexcept
    {
        const vec3<T> result = {lhs[0] - rhs[0], lhs[1] - rhs[1], lhs[2] - rhs[2]};
        return result;
    }

    template <typename T>
    inline constexpr vec3<T> operator*(const vec3<T>& lhs, const vec3<T>& rhs) noexcept
    {
        const vec3<T> result = {lhs[0] * rhs[0], lhs[1] * rhs[1], lhs[2] * rhs[2]};
        return result;
    }

    template <typename T>
    inline constexpr vec3<T> operator*(const T scalar, const vec3<T>& rhs) noexcept
    {
        const vec3<T> result = {scalar * rhs[0], scalar * rhs[1], scalar * rhs[2]};
        return result;
    }

    template <typename T>
    inline constexpr vec3<T> operator*(const vec3<T>& lhs, const T scalar) noexcept
    {
        const vec3<T> result = {scalar * lhs[0], scalar * lhs[1], scalar * lhs[2]};
        return result;
    }

    template <typename T>
    inline constexpr vec3<T> operator/(const vec3<T>& lhs, const vec3<T>& rhs) noexcept
    {
        const vec3<T> result = {lhs[0] / rhs[0], lhs[1] / rhs[1], lhs[2] / rhs[2]};
        return result;
    }

    template <typename T>
    inline constexpr vec3<T> operator/(const vec3<T>& lhs, const T scalar) noexcept
    {
        const vec3<T> result = {lhs[0] / scalar, lhs[1] / scalar, lhs[2] / scalar};
        return result;
    }

    template <typename T>
    inline constexpr vec3<T> operator-(const vec3<T>& v) noexcept
    {
        return vec3<T>(-v[0], -v[1], -v[2]);
    }

    template <typename T>
    inline constexpr T norm(const vec3<T>& v)
    {
        const T mag_squared = v.x * v.x + v.y * v.y + v.z * v.z;
        return std::sqrt(mag_squared);
    }

    template <typename T>
    inline constexpr vec3<T> normalize(const vec3<T>& v)
    {
        return v / norm(v);
    }

    template <typename T>
    inline constexpr vec3<T> cross(const vec3<T>& lhs, const vec3<T>& rhs)
    {
        return vec3(lhs[1] * rhs[2] - rhs[1] * lhs[2], lhs[2] * rhs[0] - rhs[2] * lhs[0],
                    lhs[0] * rhs[1] - rhs[0] * lhs[1]);
    }

    template <typename T>
    inline constexpr T dot(const vec3<T>& lhs, const vec3<T>& rhs)
    {
        const auto prod = lhs * rhs;
        return prod[0] + prod[1] + prod[2];
    }

    template <typename T>
    inline constexpr vec3<T> as_radians(const vec3<T>& v)
    {
        return vec3(as_radians(v.x), as_radians(v.y), as_radians(v.z));
    }

    template <typename T>
    inline constexpr vec3<T> as_degrees(const vec3<T>& v)
    {
        return vec3(as_degrees(v.x), as_degrees(v.y), as_degrees(v.z));
    }
} // namespace tempest::math

#endif // tempest_math_vec3_hpp__
