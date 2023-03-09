#ifndef tempest_intrinsic_type_hpp__
#define tempest_intrinsic_type_hpp__

#include <cstddef>
#include <cstdint>
#include <immintrin.h>

namespace tempest::math::simd
{
    template <typename T, std::size_t Count> struct intrinsic_type;

    template <> struct intrinsic_type<std::uint32_t, 2>
    {
        using type = __m128i;
    };

    template <> struct intrinsic_type<std::uint32_t, 3>
    {
        using type = __m128i;
    };

    template <> struct intrinsic_type<std::uint32_t, 4>
    {
        using type = __m128i;
    };

    template <> struct intrinsic_type<std::int32_t, 2>
    {
        using type = __m128i;
    };

    template <> struct intrinsic_type<std::int32_t, 3>
    {
        using type = __m128i;
    };

    template <> struct intrinsic_type<std::int32_t, 4>
    {
        using type = __m128i;
    };

    template <> struct intrinsic_type<float, 2>
    {
        using type = __m128;
    };

    template <> struct intrinsic_type<float, 3>
    {
        using type = __m128;
    };

    template <> struct intrinsic_type<float, 4>
    {
        using type = __m128;
    };

    template <> struct intrinsic_type<double, 2>
    {
        using type = __m128d;
    };

    template <> struct intrinsic_type<double, 3>
    {
        using type = __m256d;
    };

    template <> struct intrinsic_type<double, 4>
    {
        using type = __m256d;
    };

    template <typename T, std::size_t C> using intrinsic_type_t = intrinsic_type<T, C>::type;

    template <typename T, std::size_t C> inline void zero(T (&dst)[C]) noexcept;

    template <> inline void zero<float, 4>(float (&dst)[4]) noexcept
    {
        _mm_store_ps(dst, _mm_setzero_ps());
    }

    template <> inline void zero<double, 2>(double (&dst)[2]) noexcept
    {
        _mm_store_pd(dst, _mm_setzero_pd());
    }

    template <> inline void zero<double, 4>(double (&dst)[4]) noexcept
    {
        _mm256_store_pd(dst, _mm256_setzero_pd());
    }

    template <> inline void zero<std::int32_t, 4>(std::int32_t (&dst)[4]) noexcept
    {
        _mm_store_si128(reinterpret_cast<__m128i*>(dst), _mm_setzero_si128());
    }

    template <> inline void zero<std::uint32_t, 4>(std::uint32_t (&dst)[4]) noexcept
    {
        _mm_storeu_si128(reinterpret_cast<__m128i*>(dst), _mm_setzero_si128());
    }

    template <typename T, std::size_t C> inline intrinsic_type_t<T, C> load(const T (&data)[C]) noexcept;

    template <> inline __m128 load<float, 4>(const float (&data)[4]) noexcept
    {
        return _mm_load_ps(data);
    }

    template <> inline __m128d load<double, 2>(const double (&data)[2]) noexcept
    {
        return _mm_load_pd(data);
    }

    template <> inline __m256d load<double, 4>(const double (&data)[4]) noexcept
    {
        return _mm256_load_pd(data);
    }

    template <> inline __m128i load<std::int32_t, 4>(const std::int32_t (&data)[4]) noexcept
    {
        return _mm_load_si128(reinterpret_cast<const __m128i*>(data));
    }

    template <> inline __m128i load<std::uint32_t, 4>(const std::uint32_t (&data)[4]) noexcept
    {
        return _mm_loadu_si128(reinterpret_cast<const __m128i*>(data));
    }

    template <typename T, std::size_t C> inline void store(intrinsic_type_t<T, C> src, T (&dst)[C]) noexcept;

    template <> inline void store<float, 4>(intrinsic_type_t<float, 4> src, float (&dst)[4]) noexcept
    {
        _mm_store_ps(dst, src);
    }

    template <> inline void store<double, 2>(intrinsic_type_t<double, 2> src, double (&dst)[2]) noexcept
    {
        _mm_store_pd(dst, src);
    }

    template <> inline void store<double, 4>(intrinsic_type_t<double, 4> src, double (&dst)[4]) noexcept
    {
        _mm256_store_pd(dst, src);
    }

    template <> inline void store<std::int32_t, 4>(intrinsic_type_t<std::int32_t, 4> src, std::int32_t (&dst)[4]) noexcept
    {
        _mm_store_si128(reinterpret_cast<__m128i*>(dst), src);
    }

    template <>
    inline void store<std::uint32_t, 4>(intrinsic_type_t<std::uint32_t, 4> src, std::uint32_t (&dst)[4]) noexcept
    {
        _mm_storeu_si128(reinterpret_cast<__m128i*>(dst), src);
    }

    template <typename T, std::size_t C>
    inline bool compare_equal(intrinsic_type_t<T, C> a, intrinsic_type_t<T, C> b) noexcept;

    template <> inline bool compare_equal<float, 4>(intrinsic_type_t<float, 4> a, intrinsic_type_t<float, 4> b) noexcept
    {
        return ((_mm_movemask_ps(_mm_cmpeq_ps(a, b)) & 0b1111) == 0b1111) != 0;
    }

    template <>
    inline bool compare_equal<double, 2>(intrinsic_type_t<double, 2> a, intrinsic_type_t<double, 2> b) noexcept
    {
        return ((_mm_movemask_pd(_mm_cmpeq_pd(a, b)) & 0b11) == 0b11) != 0;
    }

    template <>
    inline bool compare_equal<double, 4>(intrinsic_type_t<double, 4> a, intrinsic_type_t<double, 4> b) noexcept
    {
        return ((_mm256_movemask_pd(_mm256_cmp_pd(a, b, 0)) & 0b1111) == 0b1111) != 0;
    }

    template <>
    inline bool compare_equal<std::int32_t, 4>(intrinsic_type_t<std::int32_t, 4> a,
                                               intrinsic_type_t<std::int32_t, 4> b) noexcept
    {
        return _mm_movemask_epi8(_mm_cmpeq_epi32(a, b)) == 0xFFFF;
    }

    template <>
    inline bool compare_equal<std::uint32_t, 4>(intrinsic_type_t<std::uint32_t, 4> a,
                                               intrinsic_type_t<std::uint32_t, 4> b) noexcept
    {
        return _mm_movemask_epi8(_mm_cmpeq_epi32(a, b)) == 0xFFFF;
    }

    template <typename T, std::size_t C>
    inline bool compare_nequal(intrinsic_type_t<T, C> a, intrinsic_type_t<T, C> b) noexcept;

    template <>
    inline bool compare_nequal<float, 4>(intrinsic_type_t<float, 4> a, intrinsic_type_t<float, 4> b) noexcept
    {
        return ((_mm_movemask_ps(_mm_cmpeq_ps(a, b)) & 0b1111) == 0b1111) == 0;
    }

    template <>
    inline bool compare_nequal<double, 2>(intrinsic_type_t<double, 2> a, intrinsic_type_t<double, 2> b) noexcept
    {
        return ((_mm_movemask_pd(_mm_cmpeq_pd(a, b)) & 0b11) == 0b11) == 0;
    }

    template <>
    inline bool compare_nequal<double, 4>(intrinsic_type_t<double, 4> a, intrinsic_type_t<double, 4> b) noexcept
    {
        return ((_mm256_movemask_pd(_mm256_cmp_pd(a, b, 0)) & 0b1111) == 0b1111) == 0;
    }

    template <>
    inline bool compare_nequal<std::int32_t, 4>(intrinsic_type_t<std::int32_t, 4> a,
                                               intrinsic_type_t<std::int32_t, 4> b) noexcept
    {
        return _mm_movemask_epi8(_mm_cmpeq_epi32(a, b)) != 0xFFFF;
    }

    template <>
    inline bool compare_nequal<std::uint32_t, 4>(intrinsic_type_t<std::uint32_t, 4> a,
                                                intrinsic_type_t<std::uint32_t, 4> b) noexcept
    {
        return _mm_movemask_epi8(_mm_cmpeq_epi32(a, b)) != 0xFFFF;
    }

    template <typename T, std::size_t C>
    inline intrinsic_type_t<T, C> add(intrinsic_type_t<T, C> a, intrinsic_type_t<T, C> b) noexcept;

    template <>
    inline intrinsic_type_t<float, 4> add<float, 4>(intrinsic_type_t<float, 4> a, intrinsic_type_t<float, 4> b) noexcept
    {
        return _mm_add_ps(a, b);
    }

    template <>
    inline intrinsic_type_t<double, 2> add<double, 2>(intrinsic_type_t<double, 2> a, intrinsic_type_t<double, 2> b) noexcept
    {
        return _mm_add_pd(a, b);
    }

    template <>
    inline intrinsic_type_t<double, 4> add<double, 4>(intrinsic_type_t<double, 4> a, intrinsic_type_t<double, 4> b) noexcept
    {
        return _mm256_add_pd(a, b);
    }

    template <>
    inline intrinsic_type_t<std::int32_t, 4> add<std::int32_t, 4>(intrinsic_type_t<std::int32_t, 4> a,
                                               intrinsic_type_t<std::int32_t, 4> b) noexcept
    {
        return _mm_add_epi32(a, b);
    }

    template <>
    inline intrinsic_type_t<std::uint32_t, 4> add<std::uint32_t, 4>(intrinsic_type_t<std::uint32_t, 4> a,
                                                                  intrinsic_type_t<std::uint32_t, 4> b) noexcept
    {
        return _mm_add_epi32(a, b);
    }

    template <typename T, std::size_t C>
    inline intrinsic_type_t<T, C> sub(intrinsic_type_t<T, C> a, intrinsic_type_t<T, C> b) noexcept;

    template <>
    inline intrinsic_type_t<float, 4> sub<float, 4>(intrinsic_type_t<float, 4> a, intrinsic_type_t<float, 4> b) noexcept
    {
        return _mm_sub_ps(a, b);
    }

    template <>
    inline intrinsic_type_t<double, 2> sub<double, 2>(intrinsic_type_t<double, 2> a,
                                                      intrinsic_type_t<double, 2> b) noexcept
    {
        return _mm_sub_pd(a, b);
    }

    template <>
    inline intrinsic_type_t<double, 4> sub<double, 4>(intrinsic_type_t<double, 4> a,
                                                      intrinsic_type_t<double, 4> b) noexcept
    {
        return _mm256_sub_pd(a, b);
    }

    template <>
    inline intrinsic_type_t<std::int32_t, 4> sub<std::int32_t, 4>(intrinsic_type_t<std::int32_t, 4> a,
                                                                  intrinsic_type_t<std::int32_t, 4> b) noexcept
    {
        return _mm_sub_epi32(a, b);
    }

    template <>
    inline intrinsic_type_t<std::uint32_t, 4> sub<std::uint32_t, 4>(intrinsic_type_t<std::uint32_t, 4> a,
                                                                  intrinsic_type_t<std::uint32_t, 4> b) noexcept
    {
        return _mm_sub_epi32(a, b);
    }

    template <typename T, std::size_t C>
    inline intrinsic_type_t<T, C> div(intrinsic_type_t<T, C> a, intrinsic_type_t<T, C> b) noexcept;

    template <>
    inline intrinsic_type_t<float, 4> div<float, 4>(intrinsic_type_t<float, 4> a, intrinsic_type_t<float, 4> b) noexcept
    {
        return _mm_div_ps(a, b);
    }

    template <>
    inline intrinsic_type_t<double, 2> div<double, 2>(intrinsic_type_t<double, 2> a,
                                                      intrinsic_type_t<double, 2> b) noexcept
    {
        return _mm_div_pd(a, b);
    }

    template <>
    inline intrinsic_type_t<double, 4> div<double, 4>(intrinsic_type_t<double, 4> a,
                                                      intrinsic_type_t<double, 4> b) noexcept
    {
        return _mm256_div_pd(a, b);
    }

    template <>
    inline intrinsic_type_t<std::int32_t, 4> div<std::int32_t, 4>(intrinsic_type_t<std::int32_t, 4> a,
                                                                  intrinsic_type_t<std::int32_t, 4> b) noexcept
    {
        return _mm_div_epi32(a, b);
    }

    template <>
    inline intrinsic_type_t<std::uint32_t, 4> div<std::uint32_t, 4>(intrinsic_type_t<std::uint32_t, 4> a,
                                                                  intrinsic_type_t<std::uint32_t, 4> b) noexcept
    {
        return _mm_div_epi32(a, b);
    }

    template <typename T, std::size_t C>
    inline intrinsic_type_t<T, C> mul(intrinsic_type_t<T, C> a, intrinsic_type_t<T, C> b) noexcept;

    template <>
    inline intrinsic_type_t<float, 4> mul<float, 4>(intrinsic_type_t<float, 4> a, intrinsic_type_t<float, 4> b) noexcept
    {
        return _mm_mul_ps(a, b);
    }

    template <>
    inline intrinsic_type_t<double, 2> mul<double, 2>(intrinsic_type_t<double, 2> a,
                                                      intrinsic_type_t<double, 2> b) noexcept
    {
        return _mm_mul_pd(a, b);
    }

    template <>
    inline intrinsic_type_t<double, 4> mul<double, 4>(intrinsic_type_t<double, 4> a,
                                                      intrinsic_type_t<double, 4> b) noexcept
    {
        return _mm256_mul_pd(a, b);
    }

    template <>
    inline intrinsic_type_t<std::int32_t, 4> mul<std::int32_t, 4>(intrinsic_type_t<std::int32_t, 4> a,
                                                                  intrinsic_type_t<std::int32_t, 4> b) noexcept
    {
        return _mm_mul_epi32(a, b);
    }

    template <>
    inline intrinsic_type_t<std::uint32_t, 4> mul<std::uint32_t, 4>(intrinsic_type_t<std::uint32_t, 4> a,
                                                                  intrinsic_type_t<std::uint32_t, 4> b) noexcept
    {
        return _mm_mul_epi32(a, b);
    }

    template <typename T, std::size_t C>
    inline T dot(intrinsic_type_t<T, C> a, intrinsic_type_t<T, C> b) noexcept;

    template <>
    inline float dot<float, 4>(intrinsic_type_t<float, 4> a, intrinsic_type_t<float, 4> b) noexcept
    {
        return _mm_cvtss_f32(_mm_dp_ps(a, b, 0xff));
    }

    template <>
    inline double dot<double, 2>(intrinsic_type_t<double, 2> a,
                                                      intrinsic_type_t<double, 2> b) noexcept
    {
        return _mm_cvtsd_f64(_mm_dp_pd(a, b, 0xff));
    }

    template <>
    inline double dot<double, 4>(intrinsic_type_t<double, 4> a,
                                                      intrinsic_type_t<double, 4> b) noexcept
    {
        intrinsic_type_t<double, 4> temp = _mm256_mul_pd(a, b);
        temp = _mm256_hadd_pd(temp, temp);
        return _mm256_cvtsd_f64(_mm256_hadd_pd(temp, temp));
    }

    template <>
    inline std::int32_t dot<std::int32_t, 4>(intrinsic_type_t<std::int32_t, 4> a,
                                                                  intrinsic_type_t<std::int32_t, 4> b) noexcept
    {
        intrinsic_type_t<std::int32_t, 4> temp = _mm_mul_epi32(a, b);
        temp = _mm_hadd_epi32(temp, temp);
        return _mm_cvtsi128_si32(_mm_hadd_epi32(temp, temp));
    }

    template <>
    inline std::uint32_t dot<std::uint32_t, 4>(intrinsic_type_t<std::uint32_t, 4> a,
                                             intrinsic_type_t<std::uint32_t, 4> b) noexcept
    {
        intrinsic_type_t<std::uint32_t, 4> temp = _mm_mul_epi32(a, b);
        temp = _mm_hadd_epi32(temp, temp);
        return _mm_cvtsi128_si32(_mm_hadd_epi32(temp, temp));
    }

    template <typename T, std::size_t C>
    inline intrinsic_type_t<T, C> cross(intrinsic_type_t<T, C> a, intrinsic_type_t<T, C> b) noexcept;

    template <>
    inline intrinsic_type_t<float, 4> cross<float, 4>(intrinsic_type_t<float, 4> a,
                                                      intrinsic_type_t<float, 4> b) noexcept
    {
        auto tmp0 = _mm_shuffle_ps(b, b, _MM_SHUFFLE(3, 0, 2, 1));
        auto tmp1 = _mm_shuffle_ps(a, a, _MM_SHUFFLE(3, 0, 2, 1));

        tmp0 = _mm_mul_ps(tmp0, a);
        tmp1 = _mm_mul_ps(tmp1, b);

        auto tmp2 = _mm_sub_ps(tmp0, tmp1);
        return _mm_shuffle_ps(tmp2, tmp2, _MM_SHUFFLE(3, 0, 2, 1));
    }

    template <>
    inline intrinsic_type_t<double, 4> cross<double, 4>(intrinsic_type_t<double, 4> a,
                                                      intrinsic_type_t<double, 4> b) noexcept
    {
        /// TODO: Check the shuffle values
        auto tmp0 = _mm256_shuffle_pd(b, b, _MM_SHUFFLE(3, 0, 2, 1));
        auto tmp1 = _mm256_shuffle_pd(a, a, _MM_SHUFFLE(3, 0, 2, 1));

        tmp0 = _mm256_mul_pd(tmp0, a);
        tmp1 = _mm256_mul_pd(tmp1, b);

        auto tmp2 = _mm256_sub_pd(tmp0, tmp1);
        return _mm256_shuffle_pd(tmp2, tmp2, _MM_SHUFFLE(3, 0, 2, 1));
    }

    template <>
    inline intrinsic_type_t<std::int32_t, 4> cross<std::int32_t, 4>(intrinsic_type_t<std::int32_t, 4> a,
                                                                  intrinsic_type_t<std::int32_t, 4> b) noexcept
    {
        /// TODO: Check if this is correct
        auto tmp0 = _mm_shuffle_epi32(b, _MM_SHUFFLE(3, 0, 2, 1));
        auto tmp1 = _mm_shuffle_epi32(a, _MM_SHUFFLE(3, 0, 2, 1));

        tmp0 = _mm_mul_epi32(tmp0, a);
        tmp1 = _mm_mul_epi32(tmp1, b);

        auto tmp2 = _mm_sub_epi32(tmp0, tmp1);
        return _mm_shuffle_epi32(tmp2, _MM_SHUFFLE(3, 0, 2, 1));
    }

    template <>
    inline intrinsic_type_t<std::uint32_t, 4> cross<std::uint32_t, 4>(intrinsic_type_t<std::uint32_t, 4> a,
                                                                    intrinsic_type_t<std::uint32_t, 4> b) noexcept
    {
        /// TODO: Check if this is correct
        auto tmp0 = _mm_shuffle_epi32(b, _MM_SHUFFLE(3, 0, 2, 1));
        auto tmp1 = _mm_shuffle_epi32(a, _MM_SHUFFLE(3, 0, 2, 1));

        tmp0 = _mm_mul_epi32(tmp0, a);
        tmp1 = _mm_mul_epi32(tmp1, b);

        auto tmp2 = _mm_sub_epi32(tmp0, tmp1);
        return _mm_shuffle_epi32(tmp2, _MM_SHUFFLE(3, 0, 2, 1));
    }

} // namespace tempest::math::simd

#endif // tempest_intrinsic_type_hpp__