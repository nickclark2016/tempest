#ifndef tempest_mat_hpp
#define tempest_mat_hpp

#include <tempest/intrinsic_type.hpp>
#include <tempest/simd_align.hpp>
#include <tempest/vec.hpp>

#include <cmath>

namespace tempest::math
{
    template <typename T, std::size_t Col, std::size_t Row> struct mat;

    template <typename T> struct alignas(simd::simd_align<T>::value) mat<T, 4, 4>
    {
      public:
        using intrinsic_type = simd::intrinsic_type_t<T, 4>;

        inline constexpr mat() noexcept = default;

        inline constexpr mat(T d) noexcept : data{d, 0, 0, 0, 0, d, 0, 0, 0, 0, d, 0, 0, 0, 0, d}
        {
        }

        inline constexpr mat(T m11, T m12, T m13, T m14, T m21, T m22, T m23, T m24, T m31, T m32, T m33, T m34, T m41,
                             T m42, T m43, T m44) noexcept
            : data{m11, m12, m13, m14, m21, m22, m23, m24, m31, m32, m33, m34, m41, m42, m43, m44}
        {
        }

        inline constexpr mat(const vec<T, 4>& col1, const vec<T, 4>& col2, const vec<T, 4>& col3,
                             const vec<T, 4>& col4) noexcept
            : data{col1.data[0], col1.data[1], col1.data[2], col1.data[3], col2.data[0], col2.data[1],
                   col2.data[2], col2.data[3], col3.data[0], col3.data[1], col3.data[2], col3.data[3],
                   col4.data[0], col4.data[1], col4.data[2], col4.data[3]}
        {
        }

        inline constexpr mat(const mat& other) noexcept
            : data{other.data[0],  other.data[1],  other.data[2],  other.data[3], other.data[4],  other.data[5],
                   other.data[6],  other.data[7],  other.data[8],  other.data[9], other.data[10], other.data[11],
                   other.data[12], other.data[13], other.data[14], other.data[15]}
        {
        }

        inline constexpr mat(mat&& other) noexcept
            : data{std::move(other.data[0]),  std::move(other.data[1]),  std::move(other.data[2]),
                   std::move(other.data[3]),  std::move(other.data[4]),  std::move(other.data[5]),
                   std::move(other.data[6]),  std::move(other.data[7]),  std::move(other.data[8]),
                   std::move(other.data[9]),  std::move(other.data[10]), std::move(other.data[11]),
                   std::move(other.data[12]), std::move(other.data[13]), std::move(other.data[14]),
                   std::move(other.data[15])}
        {
        }

        inline static constexpr mat identity() noexcept
        {
            return mat(1);
        }

        inline constexpr mat& operator=(const mat& other) noexcept
        {
            if (std::is_constant_evaluated())
            {
                for (std::size_t i = 0; i < 4 * 4; i++)
                {
                    data[i] = other.data[i];
                }
            }
            else
            {
                for (std::size_t i = 0; i < 4; i++)
                {
                    intrincolumn[i] = other.intrincolumn[i];
                }
            }

            return *this;
        }

        inline constexpr mat& operator=(mat&& other) noexcept
        {
            if (std::is_constant_evaluated())
            {
                for (std::size_t i = 0; i < 4 * 4; i++)
                {
                    data[i] = std::move(other.data[i]);
                }
            }
            else
            {
                for (std::size_t i = 0; i < 4; i++)
                {
                    intrincolumn[i] = std::move(other.intrincolumn[i]);
                }
            }

            return *this;
        }

        inline constexpr T& operator[](const std::size_t index) noexcept
        {
            return data[index];
        }

        inline constexpr const T& operator[](const std::size_t index) const noexcept
        {
            return data[index];
        }

        union {
            struct
            {
                T m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16;
            };

            struct
            {
                T m11, m12, m13, m14, m21, m22, m23, m24, m31, m32, m33, m34, m41, m42, m43, m44;
            };

            struct
            {
                vec<T, 4> col[4];
            };

            simd::mat_storage_type_t<T, 4, 4> data = {};

            struct
            {
                simd::intrinsic_type_t<T, 4> intrincolumn[4];
            };
        };
    };

    template <typename T, std::size_t Col, std::size_t Row>
    inline constexpr bool operator==(const mat<T, Col, Row>& lhs, const mat<T, Col, Row>& rhs) noexcept
    {
        if (std::is_constant_evaluated())
        {
            for (std::size_t i = 0; i < Col * Row; i++)
            {
                if (lhs[i] != rhs[i])
                {
                    return false;
                }
            }

            return true;
        }
        else
        {
            for (std::size_t i = 0; i < Col; i++)
            {
                if (!simd::compare_equal<T, simd::storage_type_size<T, Col>>(lhs.intrincol[i], rhs.intrincol[i]))
                {
                    return false;
                }
            }

            return true;
        }
    }

    template <typename T, std::size_t Col, std::size_t Row>
    inline constexpr bool operator!=(const mat<T, Col, Row>& lhs, const mat<T, Col, Row>& rhs) noexcept
    {
        if (std::is_constant_evaluated())
        {
            for (std::size_t i = 0; i < Col * Row; i++)
            {
                if (lhs[i] != rhs[i])
                {
                    return true;
                }
            }

            return true;
        }
        else
        {
            for (std::size_t i = 0; i < Col; i++)
            {
                if (!simd::compare_equal<T, simd::storage_type_size<T, Col>>(lhs.intrincol[i], rhs.intrincol[i]))
                {
                    return true;
                }
            }

            return false;
        }
    }

    template <typename T, std::size_t Col, std::size_t Row>
    inline constexpr mat<T, Col, Row> operator+(const mat<T, Col, Row>& lhs, const mat<T, Col, Row>& rhs) noexcept
    {
        if (std::is_constant_evaluated())
        {
            vec<T, Col> res;

            for (std::size_t i = 0; i < Col; i++)
            {
                res[i] = lhs[i] + rhs[i];
            }

            return res;
        }
        else
        {
            vec<T, Col> res;
            for (std::size_t i = 0; i < Col; i++)
            {
                res.intrincol[i] = simd::add<T, simd::storage_type_size<T, Col>>(lhs.intrincol[i], rhs.intrincol[i]);
            }

            return res;
        }
    }

    template <typename T, std::size_t Col, std::size_t Row>
    inline constexpr mat<T, Col, Row>& operator+=(mat<T, Col, Row>& lhs, const mat<T, Col, Row>& rhs) noexcept
    {
        if (std::is_constant_evaluated())
        {
            for (std::size_t i = 0; i < Col * Row; i++)
            {
                lhs[i] += rhs[i];
            }
        }
        else
        {
            for (std::size_t i = 0; i < Col; i++)
            {
                lhs.intrincol[i] = simd::add<T, simd::storage_type_size<T, Col>>(lhs.intrincol[i], rhs.intrincol[i]);
            }
        }

        return lhs;
    }

    template <typename T, std::size_t Col, std::size_t Row>
    inline constexpr mat<T, Col, Row> operator-(const mat<T, Col, Row>& lhs, const mat<T, Col, Row>& rhs) noexcept
    {
        if (std::is_constant_evaluated())
        {
            vec<T, Col> res;

            for (std::size_t i = 0; i < Col; i++)
            {
                res[i] = lhs[i] - rhs[i];
            }

            return res;
        }
        else
        {
            vec<T, Col> res;
            for (std::size_t i = 0; i < Col; i++)
            {
                res.intrincol[i] = simd::sub<T, simd::storage_type_size<T, Col>>(lhs.intrincol[i], rhs.intrincol[i]);
            }

            return res;
        }
    }

    template <typename T, std::size_t Col, std::size_t Row>
    inline constexpr mat<T, Col, Row>& operator-=(mat<T, Col, Row>& lhs, const mat<T, Col, Row>& rhs) noexcept
    {
        if (std::is_constant_evaluated())
        {
            for (std::size_t i = 0; i < Col * Row; i++)
            {
                lhs[i] -= rhs[i];
            }
        }
        else
        {
            for (std::size_t i = 0; i < Col; i++)
            {
                lhs.intrincol[i] = simd::sub<T, simd::storage_type_size<T, Col>>(lhs.intrincol[i], rhs.intrincol[i]);
            }
        }

        return lhs;
    }

    template <typename T, std::size_t Col, std::size_t Row>
    inline constexpr mat<T, Col, Row> operator*(const mat<T, Col, Row>& lhs, const mat<T, Col, Row>& rhs) noexcept
    {
        if (std::is_constant_evaluated())
        {
            mat<T, Col, Row> res;

            return res;
        }
        else
        {
            mat<T, Col, Row> res;

            for (std::size_t i = 0; i < Col; i++)
            {
                auto element0 = simd::broadcast<T, simd::storage_type_size<T, Col>>(rhs.data + (Col * i + 0));
                auto element1 = simd::broadcast<T, simd::storage_type_size<T, Col>>(rhs.data + (Col * i + 1));
                auto element2 = simd::broadcast<T, simd::storage_type_size<T, Col>>(rhs.data + (Col * i + 2));
                auto element3 = simd::broadcast<T, simd::storage_type_size<T, Col>>(rhs.data + (Col * i + 3));

                auto result = simd::add<T, Col>(simd::add<T, Col>(simd::mul<T, Col>(element0, lhs.intrincol[0]),
                                                                  simd::mul<T, Col>(element1, lhs.intrincol[1])),
                                                simd::add<T, Col>(simd::mul<T, Col>(element2, lhs.intrincol[2]),
                                                                  simd::mul<T, Col>(element3, lhs.intrincol[3])));

                simd::store(result, res.data + Col * i);
            }

            return res;
        }
    }

    template <typename T, std::size_t Col, std::size_t Row>
    inline constexpr mat<T, Col, Row>& operator*=(mat<T, Col, Row>& lhs, const mat<T, Col, Row>& rhs) noexcept
    {
        auto cpy = lhs;
        lhs = cpy * rhs;

        return lhs;
    }

    template <typename T>
    inline mat<T, 4, 4> perspective(T near, T far, T fov_y, T aspect_ratio) noexcept
    {

        const T tan_fov_2 = std::tan(fov_y / static_cast<T>(2));
        const T inv_tan_fov_2 = static_cast<T>(1) / tan_fov_2;
        const T inv_aspect_tan_fov_2 = inv_tan_fov_2 * static_cast<T>(1) / aspect_ratio;
        const T nmf = near - far;
        const T zero = static_cast<T>(0);

        const vec<T, 4> col0(inv_aspect_tan_fov_2, zero, zero, zero);
        const vec<T, 4> col1(zero, inv_tan_fov_2, zero, zero);
        const vec<T, 4> col2(zero, zero, (-near - far) / nmf, static_cast<T>(1));
        const vec<T, 4> col3(zero, zero, (static_cast<T>(2) * near * far) / nmf, zero);

        return mat<T, 4, 4>(col0, col1, col2, col3);
    }
} // namespace tempest::math

#endif // tempest_mat_hpp