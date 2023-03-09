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

        inline constexpr explicit vec(T v) noexcept : x{v}, y{v}
        {
        }

        inline constexpr vec(T x, T y) noexcept : x{x}, y{y}
        {
        }

        inline constexpr vec(const vec& other) noexcept : x{other.x}, y{other.y}
        {
        }

        inline constexpr vec(vec&& other) noexcept : x{std::move(other.x)}, y{std::move(other.y)}
        {
        }

        inline constexpr void zero() noexcept
        {
            if (std::is_constant_evaluated())
            {
                x = y = 0;
            }
            else
            {
                simd::zero<T, simd::storage_type_size<T, 2>>(data);
            }
        }

        inline constexpr void set(T x, T y) noexcept
        {
            this->x = x;
            this->y = y;
        }

        inline constexpr vec& operator=(const vec& other) noexcept
        {
            if (std::is_constant_evaluated())
            {
                x = other.x;
                y = other.y;
            }
            else
            {
                simd::store<T, simd::storage_type_size<T, 2>>(simd::load<T, simd::storage_type_size<T, 2>>(other.data),
                                                              data);
            }

            return *this;
        }

        inline constexpr vec& operator=(vec&& other) noexcept
        {
            if (std::is_constant_evaluated())
            {
                x = std::move(other.x);
                y = std::move(other.y);
            }
            else
            {
                simd::store<T, simd::storage_type_size<T, 2>>(simd::load<T, simd::storage_type_size<T, 2>>(other.data),
                                                              data);
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
        };
    };

    template <typename T> struct alignas(simd::simd_align<T>::value) vec<T, 3>
    {
      public:
        using intrinsic_type = simd::intrinsic_type_t<T, 3>;

        inline constexpr vec() noexcept = default;

        inline constexpr vec(T x, T y, T z) noexcept : x{x}, y{y}, z{z}
        {
        }

        inline constexpr explicit vec(T v) noexcept : x{v}, y{v}, z{v}
        {
        }

        inline constexpr vec(const vec& other) noexcept : x{other.x}, y{other.y}, z{other.z}
        {
        }

        inline constexpr vec(vec&& other) noexcept : x{std::move(other.x)}, y{std::move(other.y)}, z{std::move(other.z)}
        {
        }

        inline constexpr void zero() noexcept
        {
            if (std::is_constant_evaluated())
            {
                x = y = z = 0;
            }
            else
            {
                simd::zero<T, simd::storage_type_size<T, 3>>(data);
            }
        }

        inline constexpr void set(T x, T y, T z) noexcept
        {
            this->x = x;
            this->y = y;
            this->z = z;
        }

        inline constexpr vec& operator=(const vec& other) noexcept
        {
            if (std::is_constant_evaluated())
            {
                x = other.x;
                y = other.y;
                z = other.z;
            }
            else
            {
                simd::store<T, simd::storage_type_size<T, 3>>(simd::load<T, simd::storage_type_size<T, 3>>(other.data),
                                                              data);
            }

            return *this;
        }

        inline constexpr vec& operator=(vec&& other) noexcept
        {
            if (std::is_constant_evaluated())
            {
                x = std::move(other.x);
                y = std::move(other.y);
                z = std::move(other.z);
            }
            else
            {
                simd::store<T, simd::storage_type_size<T, 3>>(simd::load<T, simd::storage_type_size<T, 3>>(other.data),
                                                              data);
            }

            return *this;
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
        };
    };

    template <typename T> struct alignas(simd::simd_align<T>::value) vec<T, 4>
    {
      public:
        using intrinsic_type = simd::intrinsic_type_t<T, 4>;

        inline constexpr vec() noexcept = default;

        inline constexpr vec(T x, T y) noexcept : x{x}, y{y}, z{z}, w{w}
        {
        }

        inline constexpr explicit vec(T v) noexcept : x{v}, y{v}, z{v}, w{v}
        {
        }

        inline constexpr vec(const vec& other) noexcept : x{other.x}, y{other.y}, z{other.z}, w{other.w}
        {
        }

        inline constexpr vec(vec&& other) noexcept
            : x{std::move(other.x)}, y{std::move(other.y)}, z{std::move(other.z)}, w{std::move(other.w)}
        {
        }

        inline constexpr void zero() noexcept
        {
            if (std::is_constant_evaluated())
            {
                x = y = z = w = 0;
            }
            else
            {
                simd::zero<T, simd::storage_type_size<T, 4>>(data);
            }
        }

        inline constexpr void set(T x, T y) noexcept
        {
            this->x = x;
            this->y = y;
            this->z = z;
            this->w = w;
        }

        inline constexpr vec& operator=(const vec& other) noexcept
        {
            if (std::is_constant_evaluated())
            {
                x = other.x;
                y = other.y;
                z = other.z;
                w = other.w;
            }
            else
            {
                simd::store<T, simd::storage_type_size<T, 4>>(simd::load<T, simd::storage_type_size<T, 4>>(other.data),
                                                              data);
            }

            return *this;
        }

        inline constexpr vec& operator=(vec&& other) noexcept
        {
            if (std::is_constant_evaluated())
            {
                x = std::move(other.x);
                y = std::move(other.y);
                z = std::move(other.z);
                w = std::move(other.w);
            }
            else
            {
                simd::store<T, simd::storage_type_size<T, 4>>(simd::load<T, simd::storage_type_size<T, 4>>(other.data),
                                                              data);
            }

            return *this;
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
        };
    };

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
            auto a = simd::load<T, simd::storage_type_size<T, D>>(lhs.data);
            auto b = simd::load<T, simd::storage_type_size<T, D>>(rhs.data);

            return simd::compare_equal<T, simd::storage_type_size<T, D>>(a, b);
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
            auto a = simd::load<T, simd::storage_type_size<T, D>>(lhs.data);
            auto b = simd::load<T, simd::storage_type_size<T, D>>(rhs.data);

            return simd::compare_nequal<T, simd::storage_type_size<T, D>>(a, b);
        }
    }

    template<typename T, std::size_t D> 
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
            auto a = simd::load<T, simd::storage_type_size<T, D>>(lhs.data);
            auto b = simd::load<T, simd::storage_type_size<T, D>>(rhs.data);

            auto v = simd::add<T, simd::storage_type_size<T, D>>(a, b);

            vec<T, D> res;
            simd::store<T, simd::storage_type_size<T, D>>(v, res.data);

            return res;
        }
    }

    template<typename T, std::size_t D> 
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
            auto a = simd::load<T, simd::storage_type_size<T, D>>(lhs.data);
            auto b = simd::load<T, simd::storage_type_size<T, D>>(rhs.data);

            auto v = simd::add<T, simd::storage_type_size<T, D>>(a, b);

            simd::store<T, simd::storage_type_size<T, D>>(v, lhs.data);
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
            auto a = simd::load<T, simd::storage_type_size<T, D>>(lhs.data);
            auto b = simd::load<T, simd::storage_type_size<T, D>>(rhs.data);

            auto v = simd::sub<T, simd::storage_type_size<T, D>>(a, b);

            vec<T, D> res;
            simd::store<T, simd::storage_type_size<T, D>>(v, res.data);

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
            auto a = simd::load<T, simd::storage_type_size<T, D>>(lhs.data);
            auto b = simd::load<T, simd::storage_type_size<T, D>>(rhs.data);

            auto v = simd::sub<T, simd::storage_type_size<T, D>>(a, b);

            simd::store<T, simd::storage_type_size<T, D>>(v, lhs.data);
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
            auto a = simd::load<T, simd::storage_type_size<T, D>>(lhs.data);
            auto b = simd::load<T, simd::storage_type_size<T, D>>(rhs.data);

            auto v = simd::div<T, simd::storage_type_size<T, D>>(a, b);

            vec<T, D> res;
            simd::store<T, simd::storage_type_size<T, D>>(v, res.data);

            return res;
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
            auto a = simd::load<T, simd::storage_type_size<T, D>>(lhs.data);
            auto b = simd::load<T, simd::storage_type_size<T, D>>(rhs.data);

            auto v = simd::div<T, simd::storage_type_size<T, D>>(a, b);

            simd::store<T, simd::storage_type_size<T, D>>(v, lhs.data);
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
            auto a = simd::load<T, simd::storage_type_size<T, D>>(lhs.data);
            auto b = simd::load<T, simd::storage_type_size<T, D>>(rhs.data);

            auto v = simd::mul<T, simd::storage_type_size<T, D>>(a, b);

            vec<T, D> res;
            simd::store<T, simd::storage_type_size<T, D>>(v, res.data);

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
            auto a = simd::load<T, simd::storage_type_size<T, D>>(lhs.data);
            auto b = simd::load<T, simd::storage_type_size<T, D>>(rhs.data);

            auto v = simd::mul<T, simd::storage_type_size<T, D>>(a, b);

            simd::store<T, simd::storage_type_size<T, D>>(v, lhs.data);
        }

        return lhs;
    }
} // namespace tempest::math

#endif // tempest_vec_hpp__