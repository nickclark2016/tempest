#ifndef tempest_math_sse_avx_hpp
#define tempest_math_sse_avx_hpp

#include "intrinsic_base.hpp"

#include <cstdint>
#include <emmintrin.h>
#include <immintrin.h>
#include <xmmintrin.h>

namespace tempest::math
{
    union alignas(8) intrinsic<float, 2, intrinsic_instruction_type::SSE_AVX> {
        float data[2];
        __m64 intrin;
    };

    union alignas(16) intrinsic<float, 3, intrinsic_instruction_type::SSE_AVX> {
        float data[3];
        __m128 intrin;
    };

    union alignas(16) intrinsic<float, 4, intrinsic_instruction_type::SSE_AVX> {
        float data[4];
        __m128 intrin;
    };

    union alignas(16) intrinsic<double, 2, intrinsic_instruction_type::SSE_AVX> {
        double data[2];
        __m128d intrin;
    };

    union alignas(32) intrinsic<double, 3, intrinsic_instruction_type::SSE_AVX> {
        double data[3];
        __m256d intrin;
    };

    union alignas(32) intrinsic<double, 4, intrinsic_instruction_type::SSE_AVX> {
        double data[4];
        __m256d intrin;
    };

    intrinsic<float, 2, intrinsic_instruction_type::SSE_AVX> add(
        intrinsic<float, 2, intrinsic_instruction_type::SSE_AVX> lhs,
        intrinsic<float, 2, intrinsic_instruction_type::SSE_AVX> rhs)
    {
        // No SIMD instruction for 2 lanes of floats
        intrinsic<float, 2, intrinsic_instruction_type::SSE_AVX> result{
            .data{lhs.data[0] + rhs.data[0], lhs.data[1] + rhs.data[1]},
        };
        return result;
    }

    intrinsic<float, 2, intrinsic_instruction_type::SSE_AVX> sub(
        intrinsic<float, 2, intrinsic_instruction_type::SSE_AVX> lhs,
        intrinsic<float, 2, intrinsic_instruction_type::SSE_AVX> rhs)
    {
        // No SIMD instruction for 2 lanes of floats
        intrinsic<float, 2, intrinsic_instruction_type::SSE_AVX> result{
            .data{lhs.data[0] - rhs.data[0], lhs.data[1] - rhs.data[1]},
        };
        return result;
    }

    intrinsic<float, 2, intrinsic_instruction_type::SSE_AVX> mul(
        intrinsic<float, 2, intrinsic_instruction_type::SSE_AVX> lhs,
        intrinsic<float, 2, intrinsic_instruction_type::SSE_AVX> rhs)
    {
        // No SIMD instruction for 2 lanes of floats
        intrinsic<float, 2, intrinsic_instruction_type::SSE_AVX> result{
            .data{lhs.data[0] * rhs.data[0], lhs.data[1] * rhs.data[1]},
        };
        return result;
    }

    intrinsic<float, 2, intrinsic_instruction_type::SSE_AVX> div(
        intrinsic<float, 2, intrinsic_instruction_type::SSE_AVX> lhs,
        intrinsic<float, 2, intrinsic_instruction_type::SSE_AVX> rhs)
    {
        // No SIMD instruction for 2 lanes of floats
        intrinsic<float, 2, intrinsic_instruction_type::SSE_AVX> result{
            .data{lhs.data[0] / rhs.data[0], lhs.data[1] / rhs.data[1]},
        };
        return result;
    }

    intrinsic<float, 3, intrinsic_instruction_type::SSE_AVX> add(
        intrinsic<float, 3, intrinsic_instruction_type::SSE_AVX> lhs,
        intrinsic<float, 3, intrinsic_instruction_type::SSE_AVX> rhs)
    {
        intrinsic<float, 3, intrinsic_instruction_type::SSE_AVX> result{
            .intrin{_mm_add_ps(lhs.intrin, rhs.intrin)},
        };

        return result;
    }

    intrinsic<float, 3, intrinsic_instruction_type::SSE_AVX> sub(
        intrinsic<float, 3, intrinsic_instruction_type::SSE_AVX> lhs,
        intrinsic<float, 3, intrinsic_instruction_type::SSE_AVX> rhs)
    {
        intrinsic<float, 3, intrinsic_instruction_type::SSE_AVX> result{
            .intrin{_mm_sub_ps(lhs.intrin, rhs.intrin)},
        };

        return result;
    }

    intrinsic<float, 3, intrinsic_instruction_type::SSE_AVX> mul(
        intrinsic<float, 3, intrinsic_instruction_type::SSE_AVX> lhs,
        intrinsic<float, 3, intrinsic_instruction_type::SSE_AVX> rhs)
    {
        intrinsic<float, 3, intrinsic_instruction_type::SSE_AVX> result{
            .intrin{_mm_mul_ps(lhs.intrin, rhs.intrin)},
        };

        return result;
    }

    intrinsic<float, 3, intrinsic_instruction_type::SSE_AVX> div(
        intrinsic<float, 3, intrinsic_instruction_type::SSE_AVX> lhs,
        intrinsic<float, 3, intrinsic_instruction_type::SSE_AVX> rhs)
    {
        rhs.data[3] = 1.0f;
        intrinsic<float, 3, intrinsic_instruction_type::SSE_AVX> result{
            .intrin{_mm_div_ps(lhs.intrin, rhs.intrin)},
        };

        return result;
    }

    intrinsic<float, 4, intrinsic_instruction_type::SSE_AVX> add(
        intrinsic<float, 4, intrinsic_instruction_type::SSE_AVX> lhs,
        intrinsic<float, 4, intrinsic_instruction_type::SSE_AVX> rhs)
    {
        intrinsic<float, 4, intrinsic_instruction_type::SSE_AVX> result{
            .intrin{_mm_add_ps(lhs.intrin, rhs.intrin)},
        };

        return result;
    }

    intrinsic<float, 4, intrinsic_instruction_type::SSE_AVX> sub(
        intrinsic<float, 4, intrinsic_instruction_type::SSE_AVX> lhs,
        intrinsic<float, 4, intrinsic_instruction_type::SSE_AVX> rhs)
    {
        intrinsic<float, 4, intrinsic_instruction_type::SSE_AVX> result{
            .intrin{_mm_sub_ps(lhs.intrin, rhs.intrin)},
        };

        return result;
    }

    intrinsic<float, 4, intrinsic_instruction_type::SSE_AVX> mul(
        intrinsic<float, 4, intrinsic_instruction_type::SSE_AVX> lhs,
        intrinsic<float, 4, intrinsic_instruction_type::SSE_AVX> rhs)
    {
        intrinsic<float, 4, intrinsic_instruction_type::SSE_AVX> result{
            .intrin{_mm_mul_ps(lhs.intrin, rhs.intrin)},
        };

        return result;
    }

    intrinsic<float, 4, intrinsic_instruction_type::SSE_AVX> div(
        intrinsic<float, 4, intrinsic_instruction_type::SSE_AVX> lhs,
        intrinsic<float, 4, intrinsic_instruction_type::SSE_AVX> rhs)
    {
        intrinsic<float, 4, intrinsic_instruction_type::SSE_AVX> result{
            .intrin{_mm_div_ps(lhs.intrin, rhs.intrin)},
        };

        return result;
    }

    intrinsic<double, 2, intrinsic_instruction_type::SSE_AVX> add(
        intrinsic<double, 2, intrinsic_instruction_type::SSE_AVX> lhs,
        intrinsic<double, 2, intrinsic_instruction_type::SSE_AVX> rhs)
    {
        intrinsic<double, 2, intrinsic_instruction_type::SSE_AVX> result{
            .intrin{_mm_add_pd(lhs.intrin, rhs.intrin)},
        };

        return result;
    }

    intrinsic<double, 2, intrinsic_instruction_type::SSE_AVX> sub(
        intrinsic<double, 2, intrinsic_instruction_type::SSE_AVX> lhs,
        intrinsic<double, 2, intrinsic_instruction_type::SSE_AVX> rhs)
    {
        intrinsic<double, 2, intrinsic_instruction_type::SSE_AVX> result{
            .intrin{_mm_sub_pd(lhs.intrin, rhs.intrin)},
        };

        return result;
    }

    intrinsic<double, 2, intrinsic_instruction_type::SSE_AVX> mul(
        intrinsic<double, 2, intrinsic_instruction_type::SSE_AVX> lhs,
        intrinsic<double, 2, intrinsic_instruction_type::SSE_AVX> rhs)
    {
        intrinsic<double, 2, intrinsic_instruction_type::SSE_AVX> result{
            .intrin{_mm_mul_pd(lhs.intrin, rhs.intrin)},
        };

        return result;
    }

    intrinsic<double, 2, intrinsic_instruction_type::SSE_AVX> div(
        intrinsic<double, 2, intrinsic_instruction_type::SSE_AVX> lhs,
        intrinsic<double, 2, intrinsic_instruction_type::SSE_AVX> rhs)
    {
        intrinsic<double, 2, intrinsic_instruction_type::SSE_AVX> result{
            .intrin{_mm_div_pd(lhs.intrin, rhs.intrin)},
        };

        return result;
    }

    intrinsic<double, 3, intrinsic_instruction_type::SSE_AVX> add(
        intrinsic<double, 3, intrinsic_instruction_type::SSE_AVX> lhs,
        intrinsic<double, 3, intrinsic_instruction_type::SSE_AVX> rhs)
    {
        intrinsic<double, 3, intrinsic_instruction_type::SSE_AVX> result{
            .intrin{_mm256_add_pd(lhs.intrin, rhs.intrin)},
        };

        return result;
    }

    intrinsic<double, 3, intrinsic_instruction_type::SSE_AVX> sub(
        intrinsic<double, 3, intrinsic_instruction_type::SSE_AVX> lhs,
        intrinsic<double, 3, intrinsic_instruction_type::SSE_AVX> rhs)
    {
        intrinsic<double, 3, intrinsic_instruction_type::SSE_AVX> result{
            .intrin{_mm256_sub_pd(lhs.intrin, rhs.intrin)},
        };

        return result;
    }

    intrinsic<double, 3, intrinsic_instruction_type::SSE_AVX> mul(
        intrinsic<double, 3, intrinsic_instruction_type::SSE_AVX> lhs,
        intrinsic<double, 3, intrinsic_instruction_type::SSE_AVX> rhs)
    {
        intrinsic<double, 3, intrinsic_instruction_type::SSE_AVX> result{
            .intrin{_mm256_mul_pd(lhs.intrin, rhs.intrin)},
        };

        return result;
    }

    intrinsic<double, 3, intrinsic_instruction_type::SSE_AVX> div(
        intrinsic<double, 3, intrinsic_instruction_type::SSE_AVX> lhs,
        intrinsic<double, 3, intrinsic_instruction_type::SSE_AVX> rhs)
    {
        rhs.data[3] = 1.0;
        intrinsic<double, 3, intrinsic_instruction_type::SSE_AVX> result{
            .intrin{_mm256_div_pd(lhs.intrin, rhs.intrin)},
        };

        return result;
    }

    intrinsic<double, 4, intrinsic_instruction_type::SSE_AVX> add(
        intrinsic<double, 4, intrinsic_instruction_type::SSE_AVX> lhs,
        intrinsic<double, 4, intrinsic_instruction_type::SSE_AVX> rhs)
    {
        intrinsic<double, 4, intrinsic_instruction_type::SSE_AVX> result{
            .intrin{_mm256_add_pd(lhs.intrin, rhs.intrin)},
        };

        return result;
    }

    intrinsic<double, 4, intrinsic_instruction_type::SSE_AVX> sub(
        intrinsic<double, 4, intrinsic_instruction_type::SSE_AVX> lhs,
        intrinsic<double, 4, intrinsic_instruction_type::SSE_AVX> rhs)
    {
        intrinsic<double, 4, intrinsic_instruction_type::SSE_AVX> result{
            .intrin{_mm256_sub_pd(lhs.intrin, rhs.intrin)},
        };

        return result;
    }

    intrinsic<double, 4, intrinsic_instruction_type::SSE_AVX> mul(
        intrinsic<double, 4, intrinsic_instruction_type::SSE_AVX> lhs,
        intrinsic<double, 4, intrinsic_instruction_type::SSE_AVX> rhs)
    {
        intrinsic<double, 4, intrinsic_instruction_type::SSE_AVX> result{
            .intrin{_mm256_mul_pd(lhs.intrin, rhs.intrin)},
        };

        return result;
    }

    intrinsic<double, 4, intrinsic_instruction_type::SSE_AVX> div(
        intrinsic<double, 4, intrinsic_instruction_type::SSE_AVX> lhs,
        intrinsic<double, 4, intrinsic_instruction_type::SSE_AVX> rhs)
    {
        intrinsic<double, 4, intrinsic_instruction_type::SSE_AVX> result{
            .intrin{_mm256_div_pd(lhs.intrin, rhs.intrin)},
        };

        return result;
    }
} // namespace tempest::math

#endif // tempest_math_sse_avx_hpp
