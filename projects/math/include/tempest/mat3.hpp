#ifndef tempest_math_mat3_hpp__
#define tempest_math_mat3_hpp__

#include <tempest/vec3.hpp>

namespace tempest::math
{
    template <typename T> struct alignas(sizeof(vec3<T>)) mat3
    {
        union {
            T data[12];
            vec3<T> columns[3];
            struct
            {
                // col 0
                T m00;
                T m10;
                T m20;
                T pad0; // padding
                // col 1
                T m01;
                T m11;
                T m21;
                T pad1; // padding
                // col2
                T m02;
                T m12;
                T m22;
                T pad2; // padding
            };
        };

        constexpr mat3();
        constexpr mat3(const T diagonal);
        constexpr mat3(const vec3<T>& col0, const vec3<T>& col1, const vec3<T>& col2);
        constexpr mat3(const T m00, const T m10, const T m20, const T m01, const T m11, const T m21, const T m02,
                       const T m12, const T m22);

        constexpr vec3<T>& operator[](const std::size_t index) noexcept;
        constexpr const vec3<T>& operator[](const std::size_t index) const noexcept;

        constexpr mat3& operator+=(const mat3& rhs) noexcept;
        constexpr mat3& operator-=(const mat3& rhs) noexcept;
        constexpr mat3& operator*=(const mat3& rhs) noexcept;
    };

    template <typename T> mat3(const T) -> mat3<T>;

    template <typename T> mat3(const vec3<T>&, const vec3<T>&, const vec3<T>&) -> mat3<T>;

    template <typename T>
    mat3(const T, const T, const T, const T, const T, const T, const T, const T, const T) -> mat3<T>;

    template <typename T> mat3(const mat3<T>&) -> mat3<T>;

    template <typename T> mat3(mat3<T>&&) -> mat3<T>;

    template <typename T> inline constexpr mat3<T>::mat3() : mat3(T())
    {
    }

    template <typename T>
    inline constexpr mat3<T>::mat3(const T diagonal)
        : mat3(vec3(diagonal, T(), T()), vec3(T(), diagonal, T()), vec3(T(), T(), diagonal))
    {
    }

    template <typename T>
    inline constexpr mat3<T>::mat3(const vec3<T>& col0, const vec3<T>& col1, const vec3<T>& col2)
        : mat3(col0[0], col0[1], col0[2], col1[0], col1[1], col1[2], col2[0], col2[1], col2[2])
    {
    }

    template <typename T>
    inline constexpr mat3<T>::mat3(const T m00, const T m10, const T m20, const T m01, const T m11, const T m21,
                                   const T m02, const T m12, const T m22)
        : m00(m00), m10(m10), m20(m20), m01(m01), m11(m11), m21(m21), m02(m02), m12(m12), m22(m22)
    {
    }

    template <typename T> inline constexpr vec3<T>& mat3<T>::operator[](const std::size_t index) noexcept
    {
        return columns[index];
    }

    template <typename T> inline constexpr const vec3<T>& mat3<T>::operator[](const std::size_t index) const noexcept
    {
        return columns[index];
    }

    template <typename T> inline constexpr mat3<T>& mat3<T>::operator+=(const mat3& rhs) noexcept
    {
        columns[0] += rhs[0];
        columns[1] += rhs[1];
        columns[2] += rhs[2];
        return *this;
    }

    template <typename T> inline constexpr mat3<T>& mat3<T>::operator-=(const mat3& rhs) noexcept
    {
        columns[0] -= rhs[0];
        columns[1] -= rhs[1];
        columns[2] -= rhs[2];
        return *this;
    }

    template <typename T> inline constexpr mat3<T>& mat3<T>::operator*=(const mat3& rhs) noexcept
    {
        const auto m00 = columns[0][0] * rhs[0][0] + columns[1][0] * rhs[0][1] + columns[2][0] * rhs[0][2];
        const auto m10 = columns[0][1] * rhs[0][0] + columns[1][1] * rhs[0][1] + columns[2][1] * rhs[0][2];
        const auto m20 = columns[0][2] * rhs[0][0] + columns[1][2] * rhs[0][1] + columns[2][2] * rhs[0][2];

        const auto m01 = columns[0][0] * rhs[1][0] + columns[1][0] * rhs[1][1] + columns[2][0] * rhs[1][2];
        const auto m11 = columns[0][1] * rhs[1][0] + columns[1][1] * rhs[1][1] + columns[2][1] * rhs[1][2];
        const auto m21 = columns[0][2] * rhs[1][0] + columns[1][2] * rhs[1][1] + columns[2][2] * rhs[1][2];

        const auto m02 = columns[0][0] * rhs[2][0] + columns[1][0] * rhs[2][1] + columns[2][0] * rhs[2][2];
        const auto m12 = columns[0][1] * rhs[2][0] + columns[1][1] * rhs[2][1] + columns[2][1] * rhs[2][2];
        const auto m22 = columns[0][2] * rhs[2][0] + columns[1][2] * rhs[2][1] + columns[2][2] * rhs[2][2];

        this->m00 = m00;
        this->m10 = m10;
        this->m20 = m20;
        this->m01 = m01;
        this->m11 = m11;
        this->m21 = m21;
        this->m02 = m02;
        this->m12 = m12;
        this->m22 = m22;

        return *this;
    }

    template <typename T> inline constexpr bool operator==(const mat3<T>& lhs, const mat3<T>& rhs) noexcept
    {
        return lhs[0] == rhs[0] && lhs[1] == rhs[1] && lhs[2] == rhs[2];
    }

    template <typename T> inline constexpr bool operator!=(const mat3<T>& lhs, const mat3<T>& rhs) noexcept
    {
        return lhs[0] != rhs[0] || lhs[1] != rhs[1] || lhs[2] != rhs[2];
    }

    template <typename T> inline constexpr mat3<T> operator+(const mat3<T>& lhs, const mat3<T>& rhs) noexcept
    {
        const auto col0 = lhs[0] + rhs[0];
        const auto col1 = lhs[1] + rhs[1];
        const auto col2 = lhs[2] + rhs[2];
        return mat3(col0, col1, col2);
    }

    template <typename T> inline constexpr mat3<T> operator-(const mat3<T>& lhs, const mat3<T>& rhs) noexcept
    {
        const auto col0 = lhs[0] - rhs[0];
        const auto col1 = lhs[1] - rhs[1];
        const auto col2 = lhs[2] - rhs[2];
        return mat3(col0, col1, col2);
    }

    template <typename T> inline constexpr mat3<T> operator*(const T lhs, const mat3<T>& rhs) noexcept
    {
        const auto col0 = lhs * rhs[0];
        const auto col1 = lhs * rhs[1];
        const auto col2 = lhs * rhs[2];
        return mat3(col0, col1, col2);
    }

    template <typename T> inline constexpr mat3<T> operator*(const mat3<T>& lhs, const mat3<T>& rhs) noexcept
    {
        const auto m00 = lhs[0][0] * rhs[0][0] + lhs[1][0] * rhs[0][1] + lhs[2][0] * rhs[0][2];
        const auto m10 = lhs[0][1] * rhs[0][0] + lhs[1][1] * rhs[0][1] + lhs[2][1] * rhs[0][2];
        const auto m20 = lhs[0][2] * rhs[0][0] + lhs[1][2] * rhs[0][1] + lhs[2][2] * rhs[0][2];

        const auto m01 = lhs[0][0] * rhs[1][0] + lhs[1][0] * rhs[1][1] + lhs[2][0] * rhs[1][2];
        const auto m11 = lhs[0][1] * rhs[1][0] + lhs[1][1] * rhs[1][1] + lhs[2][1] * rhs[1][2];
        const auto m21 = lhs[0][2] * rhs[1][0] + lhs[1][2] * rhs[1][1] + lhs[2][2] * rhs[1][2];

        const auto m02 = lhs[0][0] * rhs[2][0] + lhs[1][0] * rhs[2][1] + lhs[2][0] * rhs[2][2];
        const auto m12 = lhs[0][1] * rhs[2][0] + lhs[1][1] * rhs[2][1] + lhs[2][1] * rhs[2][2];
        const auto m22 = lhs[0][2] * rhs[2][0] + lhs[1][2] * rhs[2][1] + lhs[2][2] * rhs[2][2];

        mat3<T> result(m00, m10, m20, m01, m11, m21, m02, m12, m22);

        return result;
    }

    template <typename T> inline constexpr mat3<T> operator*(const mat3<T>& lhs, const vec3<T>& rhs) noexcept
    {
        const auto m00 = lhs[0][0] * rhs[0] + lhs[1][0] * rhs[1] + lhs[2][0] * rhs[2];
        const auto m10 = lhs[0][1] * rhs[0] + lhs[1][1] * rhs[1] + lhs[2][1] * rhs[2];
        const auto m20 = lhs[0][2] * rhs[0] + lhs[1][2] * rhs[1] + lhs[2][2] * rhs[2];

        return vec3(m00, m10, m20);
    }
} // namespace tempest::math

#endif // tempest_math_mat3_hpp__
