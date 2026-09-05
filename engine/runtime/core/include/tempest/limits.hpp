#ifndef tempest_core_limits_hpp
#define tempest_core_limits_hpp

#include <tempest/bit.hpp>
#include <tempest/concepts.hpp>
#include <tempest/int.hpp>
#include <tempest/type_traits.hpp>

namespace tempest
{
    template <typename T>
    class numeric_limits
    {
      public:
        static constexpr bool is_specialized = false;
    };

    namespace detail
    {
        constexpr float float_min_val = 1.17549435e-38F;
        constexpr float float_max_val = 3.40282347e+38F;
        constexpr float float_epsilon_val = 1.19209290e-7F;
        constexpr float float_round_error_val = 0.5F;

        constexpr double double_min_val = 2.2250738585072014e-308;
        constexpr double double_max_val = 1.7976931348623157e+308;
        constexpr double double_epsilon_val = 2.2204460492503131e-16;
        constexpr double double_round_error_val = 0.5;

        constexpr long double long_double_round_error_val = 0.5L;

        template <typename T>
        struct integral_numeric_limits_specialization
        {
            static constexpr bool is_specialized = true;
            static constexpr bool is_signed = is_signed_v<T>;
            static constexpr bool is_integer = true;
            static constexpr bool is_exact = true;
            static constexpr bool has_infinity = false;
            static constexpr bool has_quiet_NaN = false;
            static constexpr bool has_signaling_NaN = false;
            static constexpr bool is_iec559 = false;
            static constexpr bool is_bounded = true;
            static constexpr bool is_modulo = !is_signed_v<T> && !is_same_v<T, bool>;
            static constexpr int digits = is_same_v<T, bool> ? 1 : (sizeof(T) * 8) - (is_signed_v<T> ? 1 : 0);
            static constexpr int digits10 = is_same_v<T, bool> ? 0 : digits * 30103 / 100000;
            static constexpr int max_digits10 = 0;
            static constexpr int radix = 2;
            static constexpr int min_exponent = 0;
            static constexpr int min_exponent10 = 0;
            static constexpr int max_exponent = 0;
            static constexpr int max_exponent10 = 0;

            static constexpr auto min() noexcept -> T
            {
                if constexpr (is_same_v<T, bool>)
                {
                    return false;
                }
                else if constexpr (is_signed_v<T>)
                {
                    using U = make_unsigned_t<T>;
                    return static_cast<T>(U(1) << digits);
                }
                else
                {
                    return 0;
                }
            }

            static constexpr auto lowest() noexcept -> T
            {
                return min();
            }

            static constexpr auto max() noexcept -> T
            {
                if constexpr (is_same_v<T, bool>)
                {
                    return true;
                }
                else if constexpr (is_signed_v<T>)
                {
                    using U = make_unsigned_t<T>;
                    return static_cast<T>((U(1) << digits) - 1);
                }
                else
                {
                    return ~T(0);
                }
            }

            static constexpr auto epsilon() noexcept -> T
            {
                return T(0);
            }

            static constexpr auto round_error() noexcept -> T
            {
                return T(0);
            }

            static constexpr auto infinity() noexcept -> T
            {
                return T(0);
            }

            static constexpr auto quiet_NaN() noexcept -> T
            {
                return T(0);
            }

            static constexpr auto signaling_NaN() noexcept -> T
            {
                return T(0);
            }
        };
    } // namespace detail

    template <integral T>
    class numeric_limits<T> : public detail::integral_numeric_limits_specialization<T>
    {
    };

    template <>
    class numeric_limits<float>
    {
      public:
        static constexpr bool is_specialized = true;
        static constexpr bool is_signed = true;
        static constexpr bool is_integer = false;
        static constexpr bool is_exact = false;
        static constexpr bool has_infinity = true;
        static constexpr bool has_quiet_NaN = true;
        static constexpr bool has_signaling_NaN = true;
        static constexpr bool is_iec559 = true;
        static constexpr bool is_bounded = true;
        static constexpr bool is_modulo = false;
        static constexpr int digits = 24;
        static constexpr int digits10 = 6;
        static constexpr int max_digits10 = 9;
        static constexpr int radix = 2;
        static constexpr int min_exponent = -125;
        static constexpr int min_exponent10 = -37;
        static constexpr int max_exponent = 128;
        static constexpr int max_exponent10 = 38;

        static constexpr auto min() noexcept -> float
        {
            return detail::float_min_val;
        }

        static constexpr auto max() noexcept -> float
        {
            return detail::float_max_val;
        }

        static constexpr auto lowest() noexcept -> float
        {
            return -detail::float_max_val;
        }

        static constexpr auto epsilon() noexcept -> float
        {
            return detail::float_epsilon_val;
        }

        static constexpr auto round_error() noexcept -> float
        {
            return detail::float_round_error_val;
        }

        static constexpr auto infinity() noexcept -> float
        {
            return __builtin_huge_valf();
        }

        static constexpr auto quiet_NaN() noexcept -> float
        {
            return __builtin_nanf("");
        }

        static constexpr auto signaling_NaN() noexcept -> float
        {
            return __builtin_nansf("");
        }
    };

    template <>
    class numeric_limits<double>
    {
      public:
        static constexpr bool is_specialized = true;
        static constexpr bool is_signed = true;
        static constexpr bool is_integer = false;
        static constexpr bool is_exact = false;
        static constexpr bool has_infinity = true;
        static constexpr bool has_quiet_NaN = true;
        static constexpr bool has_signaling_NaN = true;
        static constexpr bool is_iec559 = true;
        static constexpr bool is_bounded = true;
        static constexpr bool is_modulo = false;
        static constexpr int digits = 53;
        static constexpr int digits10 = 15;
        static constexpr int max_digits10 = 17;
        static constexpr int radix = 2;
        static constexpr int min_exponent = -1021;
        static constexpr int min_exponent10 = -307;
        static constexpr int max_exponent = 1024;
        static constexpr int max_exponent10 = 308;

        static constexpr auto min() noexcept -> double
        {
            return detail::double_min_val;
        }

        static constexpr auto max() noexcept -> double
        {
            return detail::double_max_val;
        }

        static constexpr auto lowest() noexcept -> double
        {
            return -detail::double_max_val;
        }

        static constexpr auto epsilon() noexcept -> double
        {
            return detail::double_epsilon_val;
        }

        static constexpr auto round_error() noexcept -> double
        {
            return detail::double_round_error_val;
        }

        static constexpr auto infinity() noexcept -> double
        {
            return __builtin_huge_val();
        }

        static constexpr auto quiet_NaN() noexcept -> double
        {
            return __builtin_nan("");
        }

        static constexpr auto signaling_NaN() noexcept -> double
        {
            return __builtin_nans("");
        }
    };

    template <>
    class numeric_limits<long double>
    {
      public:
        static constexpr bool is_specialized = true;
        static constexpr bool is_signed = true;
        static constexpr bool is_integer = false;
        static constexpr bool is_exact = false;
        static constexpr bool has_infinity = true;
        static constexpr bool has_quiet_NaN = true;
        static constexpr bool has_signaling_NaN = true;
        static constexpr bool is_iec559 = true;
        static constexpr bool is_bounded = true;
        static constexpr bool is_modulo = false;
#ifdef __LDBL_MANT_DIG__
        static constexpr int digits = __LDBL_MANT_DIG__;
        static constexpr int digits10 = __LDBL_DIG__;
        static constexpr int max_digits10 = __LDBL_DECIMAL_DIG__;
        static constexpr int radix = 2;
        static constexpr int min_exponent = __LDBL_MIN_EXP__;
        static constexpr int min_exponent10 = __LDBL_MIN_10_EXP__;
        static constexpr int max_exponent = __LDBL_MAX_EXP__;
        static constexpr int max_exponent10 = __LDBL_MAX_10_EXP__;

        static constexpr auto min() noexcept -> long double
        {
            return __LDBL_MIN__;
        }

        static constexpr auto max() noexcept -> long double
        {
            return __LDBL_MAX__;
        }

        static constexpr auto lowest() noexcept -> long double
        {
            return -__LDBL_MAX__;
        }

        static constexpr auto epsilon() noexcept -> long double
        {
            return __LDBL_EPSILON__;
        }
#else
        static constexpr int digits = 53;
        static constexpr int digits10 = 15;
        static constexpr int max_digits10 = 17;
        static constexpr int radix = 2;
        static constexpr int min_exponent = -1021;
        static constexpr int min_exponent10 = -307;
        static constexpr int max_exponent = 1024;
        static constexpr int max_exponent10 = 308;

        static constexpr auto min() noexcept -> long double
        {
            return 2.2250738585072014e-308L;
        }

        static constexpr auto max() noexcept -> long double
        {
            return 1.7976931348623157e+308L;
        }

        static constexpr auto lowest() noexcept -> long double
        {
            return -1.7976931348623157e+308L;
        }

        static constexpr auto epsilon() noexcept -> long double
        {
            return 2.2204460492503131e-16L;
        }
#endif
        static constexpr auto round_error() noexcept -> long double
        {
            return detail::long_double_round_error_val;
        }

        static constexpr auto infinity() noexcept -> long double
        {
            return __builtin_huge_vall();
        }

        static constexpr auto quiet_NaN() noexcept -> long double
        {
            return __builtin_nanl("");
        }

        static constexpr auto signaling_NaN() noexcept -> long double
        {
            return __builtin_nansl("");
        }
    };
} // namespace tempest

#endif // tempest_core_limits_hpp
