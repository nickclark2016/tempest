#ifndef tempest_vec_hpp__
#define tempest_vec_hpp__

#include <tempest/intrinsic_type.hpp>
#include <tempest/simd_align.hpp>

namespace tempest::math
{
    template <typename T, std::size_t Size> struct vec;

    template <typename T> struct alignas(simd::simd_align<T>::value) vec<T, 2>
    {
      public:
        using intrinsic_type = simd::intrinsic_type_t<T, 2>;

        inline constexpr vec() noexcept = default;

        inline constexpr explicit vec(T v) noexcept : data{v, v}
        {
        }

        inline constexpr vec(T x, T y) noexcept : data{x, y}
        {
        }

        inline constexpr vec(const vec& other) noexcept : data{other.data[0], other.data[1]}
        {
        }

        inline constexpr vec(vec&& other) noexcept : data{std::move(other.data[0]), std::move(other.data[1])}
        {
        }

        inline static constexpr vec zero() noexcept
        {
            return vec();
        }

        inline void set(T x, T y) noexcept
        {
            data[0] = x;
            data[1] = y;
        }

        inline constexpr vec& operator=(const vec& other) noexcept
        {
            if (std::is_constant_evaluated())
            {
                data[0] = other.data[0];
                data[1] = other.data[1];
            }
            else
            {
                intrin = other.intrin;
            }

            return *this;
        }

        inline constexpr vec& operator=(vec&& other) noexcept
        {
            if (std::is_constant_evaluated())
            {
                data[0] = std::move(other.data[0]);
                data[1] = std::move(other.data[1]);
            }
            else
            {
                intrin = std::move(other.intrin);
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
                T x, y;
            };

            struct
            {
                T r, g;
            };

            struct
            {
                T s, t;
            };

            simd::storage_type_t<T, 2> data = {};
            simd::intrinsic_type_t<T, 2> intrin;
        };
    };

    template <typename T> struct alignas(simd::simd_align<T>::value) vec<T, 3>
    {
      public:
        using intrinsic_type = simd::intrinsic_type_t<T, 3>;

        inline constexpr vec() noexcept = default;

        inline constexpr vec(T x, T y, T z) noexcept : data{x, y, z}
        {
        }

        inline constexpr explicit vec(T v) noexcept : data{v, v, v}
        {
        }

        inline constexpr vec(const vec& other) noexcept : data{other.data[0], other.data[1], other.data[2]}
        {
        }

        inline constexpr vec(vec&& other) noexcept
            : data{std::move(other.data[0]), std::move(other.data[1]), std::move(other.data[2])}
        {
        }

        inline static constexpr vec zero() noexcept
        {
            return vec();
        }

        inline void set(T x, T y, T z) noexcept
        {
            data[0] = x;
            data[1] = y;
            data[2] = z;
        }

        inline constexpr vec& operator=(const vec& other) noexcept
        {
            if (std::is_constant_evaluated())
            {
                data[0] = other.data[0];
                data[1] = other.data[1];
                data[2] = other.data[2];
            }
            else
            {
                intrin = other.intrin;
            }

            return *this;
        }

        inline constexpr vec& operator=(vec&& other) noexcept
        {
            if (std::is_constant_evaluated())
            {
                data[0] = std::move(other.data[0]);
                data[1] = std::move(other.data[1]);
                data[2] = std::move(other.data[2]);
            }
            else
            {
                intrin = std::move(other.intrin);
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
                T x, y, z;
            };

            struct
            {
                T r, g, b;
            };

            struct
            {
                T s, t, u;
            };

            simd::storage_type_t<T, 3> data = {};
            simd::intrinsic_type_t<T, 3> intrin;
        };
    };

    template <typename T> struct alignas(simd::simd_align<T>::value) vec<T, 4>
    {
      public:
        using intrinsic_type = simd::intrinsic_type_t<T, 4>;

        inline constexpr vec() noexcept = default;

        inline constexpr vec(T x, T y, T z, T w) noexcept : data{x, y, z, w}
        {
        }

        inline constexpr explicit vec(T v) noexcept : data{v, v, v, v}
        {
        }

        inline constexpr vec(const vec& other) noexcept
            : data{other.data[0], other.data[1], other.data[2], other.data[3]}
        {
        }

        inline constexpr vec(vec&& other) noexcept
            : data{std::move(other.data[0]), std::move(other.data[1]), std::move(other.data[2]),
                   std::move(other.data[3])}
        {
        }

        inline static constexpr vec zero() noexcept
        {
            return vec();
        }

        inline constexpr void set(T x, T y, T z, T w) noexcept
        {
            data[0] = x;
            data[1] = y;
            data[2] = z;
            data[3] = w;
        }

        inline constexpr vec& operator=(const vec& other) noexcept
        {
            if (std::is_constant_evaluated())
            {
                data[0] = other.data[0];
                data[1] = other.data[1];
                data[2] = other.data[2];
                data[3] = other.data[3];
            }
            else
            {
                intrin = other.intrin;
            }

            return *this;
        }

        inline constexpr vec& operator=(vec&& other) noexcept
        {
            if (std::is_constant_evaluated())
            {
                data[0] = std::move(other.data[0]);
                data[1] = std::move(other.data[1]);
                data[2] = std::move(other.data[2]);
                data[3] = std::move(other.data[3]);
            }
            else
            {
                intrin = std::move(other.intrin);
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
                T x, y, z, w;
            };

            struct
            {
                T r, g, b, a;
            };

            struct
            {
                T s, t, u, v;
            };

            simd::storage_type_t<T, 4> data = {};
            simd::intrinsic_type_t<T, 4> intrin;
        };
    };

    using ivec2 = vec<std::int32_t, 2>;
    using ivec3 = vec<std::int32_t, 3>;
    using ivec4 = vec<std::int32_t, 4>;

    using fvec2 = vec<float, 2>;
    using fvec3 = vec<float, 3>;
    using fvec4 = vec<float, 4>;

    using dvec2 = vec<double, 2>;
    using dvec3 = vec<double, 3>;
    using dvec4 = vec<double, 4>;

    template <typename T, std::size_t D>
    inline constexpr bool operator==(const vec<T, D>& lhs, const vec<T, D>& rhs) noexcept
    {
        if (std::is_constant_evaluated())
        {
            for (std::size_t i = 0; i < D; i++)
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
            return simd::compare_equal<T, simd::storage_type_size<T, D>>(lhs.intrin, rhs.intrin);
        }
    }

    template <typename T, std::size_t D>
    inline constexpr bool operator!=(const vec<T, D>& lhs, const vec<T, D>& rhs) noexcept
    {
        if (std::is_constant_evaluated())
        {
            for (std::size_t i = 0; i < D; i++)
            {
                if (lhs[i] != rhs[i])
                {
                    return true;
                }
            }

            return false;
        }
        else
        {
            return simd::compare_nequal<T, simd::storage_type_size<T, D>>(lhs.intrin, rhs.intrin);
        }
    }

    template <typename T, std::size_t D>
    inline constexpr vec<T, D> operator+(const vec<T, D>& lhs, const vec<T, D>& rhs) noexcept
    {
        if (std::is_constant_evaluated())
        {
            vec<T, D> res;

            for (std::size_t i = 0; i < D; i++)
            {
                res[i] = lhs[i] + rhs[i];
            }

            return res;
        }
        else
        {
            vec<T, D> res;
            res.intrin = simd::add<T, simd::storage_type_size<T, D>>(lhs.intrin, rhs.intrin);

            return res;
        }
    }

    template <typename T, std::size_t D>
    inline constexpr vec<T, D>& operator+=(vec<T, D>& lhs, const vec<T, D>& rhs) noexcept
    {
        if (std::is_constant_evaluated())
        {
            for (std::size_t i = 0; i < D; i++)
            {
                lhs[i] += rhs[i];
            }
        }
        else
        {
            lhs.intrin = simd::add<T, simd::storage_type_size<T, D>>(lhs.intrin, rhs.intrin);
        }

        return lhs;
    }

    template <typename T, std::size_t D>
    inline constexpr vec<T, D> operator-(const vec<T, D>& lhs, const vec<T, D>& rhs) noexcept
    {
        if (std::is_constant_evaluated())
        {
            vec<T, D> res;

            for (std::size_t i = 0; i < D; i++)
            {
                res[i] = lhs[i] - rhs[i];
            }

            return res;
        }
        else
        {
            vec<T, D> res;
            res.intrin = simd::sub<T, simd::storage_type_size<T, D>>(lhs.intrin, rhs.intrin);

            return res;
        }
    }

    template <typename T, std::size_t D>
    inline constexpr vec<T, D>& operator-=(vec<T, D>& lhs, const vec<T, D>& rhs) noexcept
    {
        if (std::is_constant_evaluated())
        {
            for (std::size_t i = 0; i < D; i++)
            {
                lhs[i] -= rhs[i];
            }
        }
        else
        {
            lhs.intrin = simd::sub<T, simd::storage_type_size<T, D>>(lhs.intrin, rhs.intrin);
        }

        return lhs;
    }

    template <typename T, std::size_t D>
    inline constexpr vec<T, D> operator/(const vec<T, D>& lhs, const vec<T, D>& rhs) noexcept
    {
        if (std::is_constant_evaluated())
        {
            vec<T, D> res;

            for (std::size_t i = 0; i < D; i++)
            {
                res[i] = lhs[i] / rhs[i];
            }

            return res;
        }
        else
        {
            if constexpr (std::is_integral_v<T>)
            {
                vec<T, D> res;

                for (std::size_t i = 0; i < D; i++)
                {
                    res[i] = lhs[i] / rhs[i];
                }

                return res;
            }
            else
            {
                vec<T, D> res;
                res.intrin = simd::div<T, simd::storage_type_size<T, D>>(lhs.intrin, rhs.intrin);

                return res;
            }
        }
    }

    template <typename T, std::size_t D>
    inline constexpr vec<T, D>& operator/=(vec<T, D>& lhs, const vec<T, D>& rhs) noexcept
    {
        if (std::is_constant_evaluated())
        {
            for (std::size_t i = 0; i < D; i++)
            {
                lhs[i] /= rhs[i];
            }
        }
        else
        {
            lhs.intrin = simd::div<T, simd::storage_type_size<T, D>>(lhs.intrin, rhs.intrin);
        }

        return lhs;
    }

    template <typename T, std::size_t D>
    inline constexpr vec<T, D> operator*(const vec<T, D>& lhs, const vec<T, D>& rhs) noexcept
    {
        if (std::is_constant_evaluated())
        {
            vec<T, D> res;

            for (std::size_t i = 0; i < D; i++)
            {
                res[i] = lhs[i] * rhs[i];
            }

            return res;
        }
        else
        {
            vec<T, D> res;
            res.intrin = simd::mul<T, simd::storage_type_size<T, D>>(lhs.intrin, rhs.intrin);

            return res;
        }
    }

    template <typename T, std::size_t D>
    inline constexpr vec<T, D>& operator*=(vec<T, D>& lhs, const vec<T, D>& rhs) noexcept
    {
        if (std::is_constant_evaluated())
        {
            for (std::size_t i = 0; i < D; i++)
            {
                lhs[i] *= rhs[i];
            }
        }
        else
        {
            lhs.intrin = simd::mul<T, simd::storage_type_size<T, D>>(lhs.intrin, rhs.intrin);
        }

        return lhs;
    }

    template <typename T, std::size_t D>
    inline constexpr vec<T, D> cross(const vec<T, D>& lhs, const vec<T, D>& rhs) noexcept
    {
        static_assert(D >= 3, "D must be 3 or 4.");

        if (std::is_constant_evaluated())
        {
            /// TODO: impl
            vec<T, D> res;
            return res;
        }
        else
        {
            vec<T, D> res;
            res.intrin = simd::cross<T, simd::storage_type_size<T, D>>(lhs.intrin, rhs.intrin);

            return res;
        }
    }

    template <typename T, std::size_t D> inline constexpr T dot(const vec<T, D>& lhs, const vec<T, D>& rhs)
    {
        if (std::is_constant_evaluated())
        {
            T value = 0;
            for (std::size_t i = 0; i < D; i++)
            {
                value += lhs[i] * rhs[i];
            }

            return value;
        }
        else
        {
            return simd::dot<T, simd::storage_type_size<T, D>>(lhs.intrin, rhs.intrin);
        }
    }

    template <typename T, std::size_t D> inline constexpr T magnitude(const vec<T, D>& v)
    {
        if (std::is_constant_evaluated())
        {
            T dp = dot(v, v);
            return sqrt(dp);
        }
        else
        {
            return sqrt(simd::dot<T, simd::storage_type_size<T, D>>(v.intrin, v.intrin));
        }
    }

    template <typename T, std::size_t D> inline constexpr T distance(const vec<T, D>& lhs, const vec<T, D>& rhs)
    {
        auto delta = rhs - lhs;
        return magnitude(delta);
    }

    template <typename T, std::size_t D> inline constexpr T project(const vec<T, D>& lhs, const vec<T, D>& rhs)
    {
        return rhs * (dot(lhs, rhs) / dot(rhs, rhs));
    }

} // namespace tempest::math

#endif // tempest_vec_hpp__