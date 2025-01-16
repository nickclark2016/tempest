#ifndef tempest_math_vec4_hpp__
#define tempest_math_vec4_hpp__

#include <cstddef>
#include <type_traits>

namespace tempest::math
{
    template <typename T>
    struct alignas(sizeof(T) * 4) vec4
    {
#if defined(_MSC_VER) && !defined(__clang__)
#pragma warning(push)
#pragma warning(disable : 4201)
#endif
        union {
            T data[4];
            struct
            {
                T r;
                T g;
                T b;
                T a;
            };
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

        constexpr vec4() = default;
        constexpr vec4(const T scalar);
        constexpr vec4(const T x, const T y, const T z, const T w);

        constexpr T& operator[](const std::size_t index) noexcept;
        constexpr const T& operator[](const std::size_t index) const noexcept;

        vec4& operator+=(const vec4& rhs) noexcept;
        vec4& operator-=(const vec4& rhs) noexcept;
        vec4& operator*=(const vec4& rhs) noexcept;
        vec4& operator/=(const vec4& rhs) noexcept;
    };

    template <typename T>
    vec4(const T) -> vec4<T>;

    template <typename T>
    vec4(const T, const T, const T, const T) -> vec4<T>;

    template <typename T>
    vec4(const vec4<T>&) -> vec4<T>;

    template <typename T>
    vec4(vec4<T>&&) -> vec4<T>;

    // Implementation

    template <typename T>
    inline constexpr vec4<T>::vec4(const T scalar) : vec4(scalar, scalar, scalar, scalar)
    {
    }

    template <typename T>
    inline constexpr vec4<T>::vec4(const T x, const T y, const T z, const T w) : x(x), y(y), z(z), w(w)
    {
    }

    template <typename T>
    inline constexpr T& vec4<T>::operator[](const std::size_t index) noexcept
    {
        return data[index];
    }

    template <typename T>
    inline constexpr const T& vec4<T>::operator[](const std::size_t index) const noexcept
    {
        return data[index];
    }

    template <typename T>
    inline vec4<T>& vec4<T>::operator+=(const vec4& rhs) noexcept
    {
        x += rhs.x;
        y += rhs.y;
        z += rhs.z;
        w += rhs.w;
        return *this;
    }

    template <typename T>
    inline vec4<T>& vec4<T>::operator-=(const vec4& rhs) noexcept
    {
        x -= rhs.x;
        y -= rhs.y;
        z -= rhs.z;
        w -= rhs.w;
        return *this;
    }

    template <typename T>
    inline vec4<T>& vec4<T>::operator*=(const vec4& rhs) noexcept
    {
        x *= rhs.x;
        y *= rhs.y;
        z *= rhs.z;
        w *= rhs.w;
        return *this;
    }

    template <typename T>
    inline vec4<T>& vec4<T>::operator/=(const vec4& rhs) noexcept
    {
        x /= rhs.x;
        y /= rhs.y;
        z /= rhs.z;
        w /= rhs.w;
        return *this;
    }

    template <typename T>
    inline bool operator==(const vec4<T>& lhs, const vec4<T>& rhs) noexcept
    {
        return lhs[0] == rhs[0] && lhs[1] == rhs[1] && lhs[2] == rhs[2] && lhs[3] == rhs[3];
    }

    template <typename T>
    inline constexpr bool operator!=(const vec4<T>& lhs, const vec4<T>& rhs) noexcept
    {
        return lhs[0] != rhs[0] || lhs[1] != rhs[1] || lhs[2] != rhs[2] || lhs[3] != rhs[3];
    }

    template <typename T>
    inline constexpr vec4<T> operator+(const vec4<T>& lhs, const vec4<T>& rhs) noexcept
    {
        const vec4<T> result = {lhs[0] + rhs[0], lhs[1] + rhs[1], lhs[2] + rhs[2], lhs[3] + rhs[3]};
        return result;
    }

    template <typename T>
    inline constexpr vec4<T> operator-(const vec4<T>& lhs, const vec4<T>& rhs) noexcept
    {
        const vec4<T> result = {lhs[0] - rhs[0], lhs[1] - rhs[1], lhs[2] - rhs[2], lhs[3] - rhs[3]};
        return result;
    }

    template <typename T>
    inline constexpr vec4<T> operator*(const vec4<T>& lhs, const vec4<T>& rhs) noexcept
    {
        const vec4<T> result = {lhs[0] * rhs[0], lhs[1] * rhs[1], lhs[2] * rhs[2], lhs[3] * rhs[3]};
        return result;
    }

    template <typename T>
    inline constexpr vec4<T> operator*(const T scalar, const vec4<T>& rhs) noexcept
    {
        const vec4<T> result = {scalar * rhs[0], scalar * rhs[1], scalar * rhs[2], scalar * rhs[3]};
        return result;
    }

    template <typename T>
    inline constexpr vec4<T> operator*(const vec4<T>& lhs, const T scalar) noexcept
    {
        const vec4<T> result = {scalar * lhs[0], scalar * lhs[1], scalar * lhs[2], scalar * lhs[3]};
        return result;
    }

    template <typename T>
    inline constexpr vec4<T> operator/(const vec4<T>& lhs, const vec4<T>& rhs) noexcept
    {
        const vec4<T> result = {lhs[0] / rhs[0], lhs[1] / rhs[1], lhs[2] / rhs[2], lhs[3] / rhs[3]};
        return result;
    }

    template <typename T>
    inline constexpr vec4<T> operator/(const vec4<T>& lhs, const T scalar) noexcept
    {
        const vec4<T> result = {lhs[0] / scalar, lhs[1] / scalar, lhs[2] / scalar, lhs[3] / scalar};
        return result;
    }

    template <typename T>
    inline constexpr vec4<T> operator-(const vec4<T>& rhs) noexcept
    {
        const vec4<T> result = {-rhs[0], -rhs[1], -rhs[2], -rhs[3]};
        return result;
    }
} // namespace tempest::math

#endif // tempest_math_vec4_hpp__
