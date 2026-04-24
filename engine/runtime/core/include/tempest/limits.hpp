#ifndef tempest_core_limits_hpp
#define tempest_core_limits_hpp

#include <tempest/bit.hpp>
#include <tempest/concepts.hpp>
#include <tempest/int.hpp>

#ifndef CHAR_BIT
#define CHAR_BIT 8
#endif

namespace tempest
{
    template <typename T>
    class numeric_limits
    {
      public:
        inline static constexpr bool is_specialized = false;
    };

    namespace detail
    {
        template <typename T>
        struct integral_numeric_limits_specialization
        {
            inline static constexpr bool is_specialized = true;
            inline static constexpr bool is_signed = is_signed_v<T>;
            inline static constexpr bool is_integer = true;
            inline static constexpr bool is_exact = true;
            inline static constexpr bool has_infinity = false;
            inline static constexpr bool has_quiet_NaN = false;
            inline static constexpr bool has_signaling_NaN = false;
            inline static constexpr bool is_iec559 = false;
            inline static constexpr bool is_bounded = true;
            inline static constexpr bool is_modulo = true;
            inline static constexpr int digits = is_same_v<T, bool> ? 1 : sizeof(T) * 8 - (is_signed_v<T> ? 1 : 0);
            inline static constexpr int digits10 = is_same_v<T, bool> ? 0 : digits * 30103 / 100000;
            inline static constexpr int max_digits10 = 0;
            inline static constexpr int radix = 2;
            inline static constexpr int min_exponent = 0;
            inline static constexpr int min_exponent10 = 0;
            inline static constexpr int max_exponent = 0;
            inline static constexpr int max_exponent10 = 0;

            inline static constexpr T min() noexcept
            {
                if constexpr (is_same_v<T, bool>)
                {
                    return false;
                }
                else if constexpr (is_signed_v<T>)
                {
                    return T(1) << (digits - 1);
                }
                else
                {
                    return 0;
                }
            }

            inline static constexpr T max() noexcept
            {
                if constexpr (is_same_v<T, bool>)
                {
                    return true;
                }
                else if constexpr (is_signed_v<T>)
                {
                    return ~(T(1) << (digits - 1));
                }
                else
                {
                    return ~T(0);
                }
            }
        };
    }

    template <integral T>
    class numeric_limits<T> : public detail::integral_numeric_limits_specialization<T>
    {
    };

    template <>
    class numeric_limits<float>
    {
      public:
        inline static constexpr bool is_specialized = true;
        inline static constexpr bool is_signed = true;
        inline static constexpr bool is_integer = false;
        inline static constexpr bool is_exact = false;
        inline static constexpr bool has_infinity = true;
        inline static constexpr bool has_quiet_NaN = true;
        inline static constexpr bool has_signaling_NaN = true;
        inline static constexpr bool is_iec559 = true;
        inline static constexpr bool is_bounded = true;
        inline static constexpr bool is_modulo = false;
        inline static constexpr int digits = 24;
        inline static constexpr int digits10 = 6;
        inline static constexpr int max_digits10 = 9;
        inline static constexpr int radix = 2;
        inline static constexpr int min_exponent = -125;
        inline static constexpr int min_exponent10 = -37;
        inline static constexpr int max_exponent = 128;
        inline static constexpr int max_exponent10 = 38;

        inline static constexpr float min() noexcept
        {
            return 1.17549435e-38f;
        }

        inline static constexpr float max() noexcept
        {
            return 3.40282347e+38f;
        }

        inline static constexpr float lowest() noexcept
        {
            return -3.40282347e+38f;
        }

        inline static constexpr float epsilon() noexcept
        {
            return 1.19209290e-7f;
        }

        inline static constexpr float round_error() noexcept
        {
            return 0.5f;
        }

        inline static constexpr float infinity() noexcept
        {
            return __builtin_huge_valf();
        }

        inline static constexpr float quiet_NaN() noexcept
        {
            return __builtin_nanf("");
        }

        inline static constexpr float signaling_NaN() noexcept
        {
            return __builtin_nansf("");
        }
    };

    template <>
    class numeric_limits<double>
    {
      public:
        inline static constexpr bool is_specialized = true;
        inline static constexpr bool is_signed = true;
        inline static constexpr bool is_integer = false;
        inline static constexpr bool is_exact = false;
        inline static constexpr bool has_infinity = true;
        inline static constexpr bool has_quiet_NaN = true;
        inline static constexpr bool has_signaling_NaN = true;
        inline static constexpr bool is_iec559 = true;
        inline static constexpr bool is_bounded = true;
        inline static constexpr bool is_modulo = false;
        inline static constexpr int digits = 53;
        inline static constexpr int digits10 = 15;
        inline static constexpr int max_digits10 = 17;
        inline static constexpr int radix = 2;
        inline static constexpr int min_exponent = -1021;
        inline static constexpr int min_exponent10 = -307;
        inline static constexpr int max_exponent = 1024;
        inline static constexpr int max_exponent10 = 308;

        inline static constexpr double min() noexcept
        {
            return 2.2250738585072014e-308;
        }

        inline static constexpr double max() noexcept
        {
            return 1.7976931348623157e+308;
        }

        inline static constexpr double lowest() noexcept
        {
            return -1.7976931348623157e+308;
        }

        inline static constexpr double epsilon() noexcept
        {
            return 2.2204460492503131e-16;
        }

        inline static constexpr double round_error() noexcept
        {
            return 0.5;
        }

        inline static constexpr double infinity() noexcept
        {
            return __builtin_huge_val();
        }

        inline static constexpr double quiet_NaN() noexcept
        {
            return __builtin_nan("");
        }

        inline static constexpr double signaling_NaN() noexcept
        {
            return __builtin_nans("");
        }
    };
} // namespace tempest

#endif // tempest_core_limits_hpp
