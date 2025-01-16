#ifndef tempest_math_quat_hpp__
#define tempest_math_quat_hpp__

#include "math_utils.hpp"
#include "vec3.hpp"

#include <cmath>
#include <cstddef>
#include <type_traits>

namespace tempest::math
{
#if defined(_MSC_VER) && !defined(__clang__)
#pragma warning(push)
#pragma warning(disable : 4201)
#endif
    template <typename T>
    struct alignas(sizeof(T) * 4) quat
    {

        union {
            T data[4];
            struct
            {
                T x;
                T y;
                T z;
                T w;
            };
        };
#if defined(_MSC_VER) && !defined(__clang__)
#pragma warning(pop)
#endif

        constexpr quat() = default;
        constexpr quat(const T scalar);
        constexpr quat(const T x, const T y, const T z, const T w);
        constexpr quat(const vec3<T>& euler);

        constexpr T& operator[](const std::size_t index) noexcept;
        constexpr const T& operator[](const std::size_t index) const noexcept;
    };

    template <typename T>
    quat(const T) -> quat<T>;

    template <typename T>
    quat(const T, const T, const T, const T) -> quat<T>;

    template <typename T>
    quat(const vec3<T>&) -> quat<T>;

    template <typename T>
    quat(const quat<T>&) -> quat<T>;

    template <typename T>
    quat(quat<T>&&) -> quat<T>;

    template <typename T>
    inline constexpr quat<T>::quat(const T scalar) : quat(scalar, scalar, scalar, scalar)
    {
    }

    template <typename T>
    inline constexpr quat<T>::quat(const T x, const T y, const T z, const T w) : x{x}, y{y}, z{z}, w{w}
    {
    }

    template <typename T>
    inline constexpr quat<T>::quat(const vec3<T>& euler)
    {
        const vec3<T> half = static_cast<T>(0.5) * euler;
        const vec3<T> c(std::cos(half.x), std::cos(half.y), std::cos(half.z));
        const vec3<T> s(std::sin(half.x), std::sin(half.y), std::sin(half.z));

        w = c.x * c.y * c.z + s.x * s.y * s.z;
        x = s.x * c.y * c.z - c.x * s.y * s.z;
        y = c.x * s.y * c.z + s.x * c.y * s.z;
        z = c.x * c.y * s.z - s.x * s.y * c.z;
    }

    template <typename T>
    inline constexpr T& quat<T>::operator[](const std::size_t index) noexcept
    {
        return data[index];
    }

    template <typename T>
    inline constexpr const T& quat<T>::operator[](const std::size_t index) const noexcept
    {
        return data[index];
    }

    template <typename T>
    inline constexpr quat<T> operator*(const quat<T>& lhs, const quat<T>& rhs)
    {
        // w0 * w1 - x0 * x1 - y0 * y1 - z0 * z1
        // w0 * x1 + x0 * w1 + y0 * z1 - z0 * y1
        // w0 * y1 - x0 * z1 + y0 * w1 + z0 * x1
        // w0 * z1 + x0 * y1 - y0 * x1 + z0 * w1

        return quat4(lhs.w * rhs.w - lhs.x * rhs.x - lhs.y * rhs.y - lhs.z * rhs.z,
                     lhs.w * rhs.x + lhs.x * rhs.w + lhs.y * rhs.z - lhs.z * rhs.y,
                     lhs.w * rhs.y - lhs.x * rhs.z + lhs.y * rhs.w + lhs.z * rhs.x,
                     lhs.w * rhs.z + lhs.x * rhs.y - lhs.y * rhs.x + lhs.z * rhs.w);
    }

    template <typename T>
    inline constexpr quat<T> operator*(const T lhs, const quat<T>& rhs)
    {
        return quat(lhs * rhs.w, lhs * rhs.x, lhs * rhs.y, lhs * rhs.z);
    }

    template <typename T>
    inline constexpr vec3<T> operator*(const quat<T>& lhs, const vec3<T>& rhs)
    {
        T num = lhs.x * static_cast<T>(2);
        T num2 = lhs.y * static_cast<T>(2);
        T num3 = lhs.z * static_cast<T>(2);
        T num4 = lhs.x * num;
        T num5 = lhs.y * num2;
        T num6 = lhs.z * num3;
        T num7 = lhs.x * num2;
        T num8 = lhs.x * num3;
        T num9 = lhs.y * num3;
        T num10 = lhs.w * num;
        T num11 = lhs.w * num2;
        T num12 = lhs.w * num3;

        vec3<T> res;
        res.x = (static_cast<T>(1) - (num5 + num6)) * rhs.x + (num7 - num12) * rhs.y + (num8 + num11) * rhs.z;
        res.y = (num7 + num12) * rhs.x + (static_cast<T>(1) - (num4 + num6)) * rhs.y + (num9 - num10) * rhs.z;
        res.z = (num8 - num11) * rhs.x + (num9 + num10) * rhs.y + (static_cast<T>(1) - (num4 + num5)) * rhs.z;

        return res;
    }

    template <typename T>
    inline constexpr quat<T> operator-(const quat<T>& q)
    {
        return quat(-q.w, -q.x, -q.y, -q.z);
    }

    template <typename T>
    inline constexpr T norm(const quat<T>& q)
    {
        const T mag_squared = q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w;
        return std::sqrt(mag_squared);
    }

    template <typename T>
    inline constexpr quat<T> normalize(const quat<T>& q)
    {
        const T magnitude = norm(q);
        return {q.x / magnitude, q.y / magnitude, q.z / magnitude, q.w / magnitude};
    }

    template <typename T>
    inline constexpr T roll(const quat<T>& q)
    {
        T const y = static_cast<T>(2) * (q.x * q.y + q.w * q.z);
        T const x = q.w * q.w + q.x * q.x - q.y * q.y - q.z * q.z;

        if (x == static_cast<T>(0) && y == static_cast<T>(0))
        {
            return static_cast<T>(0);
        }

        return static_cast<T>(std::atan2(y, x));
    }

    template <typename T>
    inline constexpr T pitch(const quat<T>& q)
    {
        T const y = static_cast<T>(2) * (q.y * q.z + q.w * q.x);
        T const x = q.w * q.w - q.x * q.x - q.y * q.y + q.z * q.z;

        if (x == static_cast<T>(0) && y == static_cast<T>(0))
        {
            return static_cast<T>(static_cast<T>(2) * std::atan2(q.x, q.w));
        }

        return static_cast<T>(std::atan2(y, x));
    }

    template <typename T>
    inline constexpr T yaw(const quat<T>& q)
    {
        return std::asin(clamp(static_cast<T>(-2) * (q.x * q.z - q.w * q.y), static_cast<T>(-1), static_cast<T>(1)));
    }

    template <typename T>
    inline constexpr vec3<T> euler(const quat<T>& q)
    {
        const T x = pitch(q);
        const T y = yaw(q);
        const T z = roll(q);
        return vec3(x, y, z);
    }
} // namespace tempest::math

#endif // tempest_math_quat_hpp__