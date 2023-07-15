#ifndef tempest_math_mat4_hpp__
#define tempest_math_mat4_hpp__

#include "vec4.hpp"

namespace tempest::math
{
    template <typename T>
    struct alignas(sizeof(vec4<T>)) mat4
    {
        union
        {
            T data[16];
            vec4<T> columns[4];
            struct
            {
                // col 0
                T m00;
                T m10;
                T m20;
                T m30;

                // col 1
                T m01;
                T m11;
                T m21;
                T m31;

                // col 2
                T m02;
                T m12;
                T m22;
                T m32;

                // col 3
                T m03;
                T m13;
                T m23;
                T m33;
            };
        };
    
        constexpr mat4();
        constexpr mat4(const T diagonal);
        constexpr mat4(const vec4<T>& col0, const vec4<T>& col1, const vec4<T>& col2, const vec4<T>& col3);
        constexpr mat4(const T m00, const T m10, const T m20, const T m30, const T m01, const T m11, const T m21, const T m31,
            const T m02, const T m12, const T m22, const T m32, const T m03, const T m13, const T m23, const T m33);

        constexpr vec4<T>& operator[](const std::size_t col) noexcept;
        constexpr const vec4<T>& operator[](const std::size_t col) const noexcept;

        constexpr mat4& operator+=(const mat4& rhs) noexcept;
        constexpr mat4& operator-=(const mat4& rhs) noexcept;
        constexpr mat4& operator*=(const mat4& rhs) noexcept;
    };

    template <typename T>
    mat4(const T) -> mat4<T>;

    template <typename T>
    mat4(const vec4<T>&, const vec4<T>&, const vec4<T>&, const vec4<T>&) -> mat4<T>;

    template <typename T>
    mat4(const T, const T, const T, const T, const T, const T, const T, const T, const T, const T, const T, const T, const T, const T, const T, const T) -> mat4<T>;

    template <typename T>
    mat4(const mat4<T>&) -> mat4<T>;

    template <typename T>
    mat4(mat4<T>&&) -> mat4<T>;

    // Implementation

    template<typename T>
    inline constexpr mat4<T>::mat4()
        : mat4(T())
    {
    }
    
    template<typename T>
    inline constexpr mat4<T>::mat4(const T diagonal)
        : mat4(vec4<T>(diagonal, T(), T(), T()), vec4<T>(T(), diagonal, T(), T()), vec4<T>(T(), T(), diagonal, T()), vec4<T>(T(), T(), T(), diagonal))
    {
    }
    
    template<typename T>
    inline constexpr mat4<T>::mat4(const vec4<T>& col0, const vec4<T>& col1, const vec4<T>& col2, const vec4<T>& col3)
        : mat4(col0[0], col0[1], col0[2], col0[3], col1[0], col1[1], col1[2], col1[3], col2[0], col2[1], col2[2], col2[3], col3[0], col3[1], col3[2], col3[3])
    {
    }
    
    template<typename T>
    inline constexpr mat4<T>::mat4(const T m00, const T m10, const T m20, const T m30, const T m01, const T m11, const T m21, const T m31,
        const T m02, const T m12, const T m22, const T m32, const T m03, const T m13, const T m23, const T m33)
        : m00(m00), m10(m10), m20(m20), m30(m30), m01(m01), m11(m11), m21(m21), m31(m31), m02(m02), m12(m12), m22(m22), m32(m32), m03(m03), m13(m13), m23(m23), m33(m33)
    {
    }
    
    template<typename T> inline constexpr vec4<T>& mat4<T>::operator[](const std::size_t col) noexcept
    {
        return columns[col];
    }
    
    template<typename T> inline constexpr const vec4<T>& mat4<T>::operator[](const std::size_t col) const noexcept
    {
        return columns[col];
    }

    template<typename T>
    inline constexpr mat4<T>& mat4<T>::operator+=(const mat4& rhs) noexcept
    {
        columns[0] += rhs[0];
        columns[1] += rhs[1];
        columns[2] += rhs[2];
        columns[3] += rhs[3];
        return *this;
    }

    template<typename T>
    inline constexpr mat4<T>& mat4<T>::operator-=(const mat4& rhs) noexcept
    {
        columns[0] -= rhs[0];
        columns[1] -= rhs[1];
        columns[2] -= rhs[2];
        columns[3] -= rhs[3];
        return *this;
    }

    template<typename T>
    inline constexpr mat4<T>& mat4<T>::operator*=(const mat4& rhs) noexcept
    {
        const auto m00 = columns[0][0] * rhs[0][0] + columns[1][0] * rhs[0][1] + columns[2][0] * rhs[0][2] + columns[3][0] * rhs[0][3];
        const auto m10 = columns[0][1] * rhs[0][0] + columns[1][1] * rhs[0][1] + columns[2][1] * rhs[0][2] + columns[3][1] * rhs[0][3];
        const auto m20 = columns[0][2] * rhs[0][0] + columns[1][2] * rhs[0][1] + columns[2][2] * rhs[0][2] + columns[3][2] * rhs[0][3];
        const auto m30 = columns[0][3] * rhs[0][0] + columns[1][3] * rhs[0][1] + columns[2][3] * rhs[0][2] + columns[3][3] * rhs[0][3];

        const auto m01 = columns[0][0] * rhs[1][0] + columns[1][0] * rhs[1][1] + columns[2][0] * rhs[1][2] + columns[3][0] * rhs[1][3];
        const auto m11 = columns[0][1] * rhs[1][0] + columns[1][1] * rhs[1][1] + columns[2][1] * rhs[1][2] + columns[3][1] * rhs[1][3];
        const auto m21 = columns[0][2] * rhs[1][0] + columns[1][2] * rhs[1][1] + columns[2][2] * rhs[1][2] + columns[3][2] * rhs[1][3];
        const auto m31 = columns[0][3] * rhs[1][0] + columns[1][3] * rhs[1][1] + columns[2][3] * rhs[1][2] + columns[3][3] * rhs[1][3];

        const auto m02 = columns[0][0] * rhs[2][0] + columns[1][0] * rhs[2][1] + columns[2][0] * rhs[2][2] + columns[3][0] * rhs[2][3];
        const auto m12 = columns[0][1] * rhs[2][0] + columns[1][1] * rhs[2][1] + columns[2][1] * rhs[2][2] + columns[3][1] * rhs[2][3];
        const auto m22 = columns[0][2] * rhs[2][0] + columns[1][2] * rhs[2][1] + columns[2][2] * rhs[2][2] + columns[3][2] * rhs[2][3];
        const auto m32 = columns[0][3] * rhs[2][0] + columns[1][3] * rhs[2][1] + columns[2][3] * rhs[2][2] + columns[3][3] * rhs[2][3];

        const auto m03 = columns[0][0] * rhs[3][0] + columns[1][0] * rhs[3][1] + columns[2][0] * rhs[3][2] + columns[3][0] * rhs[3][3];
        const auto m13 = columns[0][1] * rhs[3][0] + columns[1][1] * rhs[3][1] + columns[2][1] * rhs[3][2] + columns[3][1] * rhs[3][3];
        const auto m23 = columns[0][2] * rhs[3][0] + columns[1][2] * rhs[3][1] + columns[2][2] * rhs[3][2] + columns[3][2] * rhs[3][3];
        const auto m33 = columns[0][3] * rhs[3][0] + columns[1][3] * rhs[3][1] + columns[2][3] * rhs[3][2] + columns[3][3] * rhs[3][3];

        this->m00 = m00;
        this->m10 = m10;
        this->m20 = m20;
        this->m30 = m30;
        this->m01 = m01;
        this->m11 = m11;
        this->m21 = m21;
        this->m31 = m31;
        this->m02 = m02;
        this->m12 = m12;
        this->m22 = m22;
        this->m32 = m32;
        this->m03 = m03;
        this->m13 = m13;
        this->m23 = m23;
        this->m33 = m33;

        return *this;
    }

    template<typename T>
    inline constexpr bool operator==(const mat4<T>& lhs, const mat4<T>& rhs) noexcept
    {
        return lhs[0] == rhs[0] && lhs[1] == rhs[1] && lhs[2] == rhs[2] && lhs[3] == rhs[3];
    }

    template<typename T>
    inline constexpr bool operator!=(const mat4<T>& lhs, const mat4<T>& rhs) noexcept
    {
        return lhs[0] != rhs[0] || lhs[1] != rhs[1] || lhs[2] != rhs[2] || lhs[3] != rhs[3];
    }

    template <typename T>
    inline constexpr mat4<T> operator+(const mat4<T>& lhs, const mat4<T>& rhs) noexcept
    {
        const auto col0 = lhs[0] + rhs[0];
        const auto col1 = lhs[1] + rhs[1];
        const auto col2 = lhs[2] + rhs[2];
        const auto col3 = lhs[3] + rhs[3];
        return mat4(col0, col1, col2, col3);
    }

    template <typename T>
    inline constexpr mat4<T> operator-(const mat4<T>& lhs, const mat4<T>& rhs) noexcept
    {
        const auto col0 = lhs[0] - rhs[0];
        const auto col1 = lhs[1] - rhs[1];
        const auto col2 = lhs[2] - rhs[2];
        const auto col3 = lhs[3] - rhs[3];
        return mat4(col0, col1, col2, col3);
    }

    template <typename T>
    inline constexpr mat4<T> operator*(const T lhs, const mat4<T>& rhs) noexcept
    {
        const auto col0 = lhs * rhs[0];
        const auto col1 = lhs * rhs[1];
        const auto col2 = lhs * rhs[2];
        const auto col3 = lhs * rhs[3];
        return mat4(col0, col1, col2, col3);
    }

    template <typename T>
    inline constexpr mat4<T> operator*(const mat4<T>& lhs, const mat4<T>& rhs) noexcept
    {
        const auto m00 = lhs[0][0] * rhs[0][0] + lhs[1][0] * rhs[0][1] + lhs[2][0] * rhs[0][2] + lhs[3][0] * rhs[0][3];
        const auto m10 = lhs[0][1] * rhs[0][0] + lhs[1][1] * rhs[0][1] + lhs[2][1] * rhs[0][2] + lhs[3][1] * rhs[0][3];
        const auto m20 = lhs[0][2] * rhs[0][0] + lhs[1][2] * rhs[0][1] + lhs[2][2] * rhs[0][2] + lhs[3][2] * rhs[0][3];
        const auto m30 = lhs[0][3] * rhs[0][0] + lhs[1][3] * rhs[0][1] + lhs[2][3] * rhs[0][2] + lhs[3][3] * rhs[0][3];

        const auto m01 = lhs[0][0] * rhs[1][0] + lhs[1][0] * rhs[1][1] + lhs[2][0] * rhs[1][2] + lhs[3][0] * rhs[1][3];
        const auto m11 = lhs[0][1] * rhs[1][0] + lhs[1][1] * rhs[1][1] + lhs[2][1] * rhs[1][2] + lhs[3][1] * rhs[1][3];
        const auto m21 = lhs[0][2] * rhs[1][0] + lhs[1][2] * rhs[1][1] + lhs[2][2] * rhs[1][2] + lhs[3][2] * rhs[1][3];
        const auto m31 = lhs[0][3] * rhs[1][0] + lhs[1][3] * rhs[1][1] + lhs[2][3] * rhs[1][2] + lhs[3][3] * rhs[1][3];

        const auto m02 = lhs[0][0] * rhs[2][0] + lhs[1][0] * rhs[2][1] + lhs[2][0] * rhs[2][2] + lhs[3][0] * rhs[2][3];
        const auto m12 = lhs[0][1] * rhs[2][0] + lhs[1][1] * rhs[2][1] + lhs[2][1] * rhs[2][2] + lhs[3][1] * rhs[2][3];
        const auto m22 = lhs[0][2] * rhs[2][0] + lhs[1][2] * rhs[2][1] + lhs[2][2] * rhs[2][2] + lhs[3][2] * rhs[2][3];
        const auto m32 = lhs[0][3] * rhs[2][0] + lhs[1][3] * rhs[2][1] + lhs[2][3] * rhs[2][2] + lhs[3][3] * rhs[2][3];

        const auto m03 = lhs[0][0] * rhs[3][0] + lhs[1][0] * rhs[3][1] + lhs[2][0] * rhs[3][2] + lhs[3][0] * rhs[3][3];
        const auto m13 = lhs[0][1] * rhs[3][0] + lhs[1][1] * rhs[3][1] + lhs[2][1] * rhs[3][2] + lhs[3][1] * rhs[3][3];
        const auto m23 = lhs[0][2] * rhs[3][0] + lhs[1][2] * rhs[3][1] + lhs[2][2] * rhs[3][2] + lhs[3][2] * rhs[3][3];
        const auto m33 = lhs[0][3] * rhs[3][0] + lhs[1][3] * rhs[3][1] + lhs[2][3] * rhs[3][2] + lhs[3][3] * rhs[3][3];

        mat4<T> result(
            m00, m10, m20, m30,
            m01, m11, m21, m31,
            m02, m12, m22, m32,
            m03, m13, m23, m33
        );

        return result;
    }

    template <typename T>
    inline constexpr vec4<T> operator*(const mat4<T>& lhs, const vec4<T>& rhs) noexcept
    {
        const auto m00 = lhs[0][0] * rhs[0] + lhs[1][0] * rhs[1] + lhs[2][0] * rhs[2] + lhs[3][0] * rhs[3];
        const auto m10 = lhs[0][1] * rhs[0] + lhs[1][1] * rhs[1] + lhs[2][1] * rhs[2] + lhs[3][1] * rhs[3];
        const auto m20 = lhs[0][2] * rhs[0] + lhs[1][2] * rhs[1] + lhs[2][2] * rhs[2] + lhs[3][2] * rhs[3];
        const auto m30 = lhs[0][3] * rhs[0] + lhs[1][3] * rhs[1] + lhs[2][3] * rhs[2] + lhs[3][3] * rhs[3];

        return vec4(m00, m10, m20, m30);
    }
}

#endif // tempest_math_mat4_hpp__