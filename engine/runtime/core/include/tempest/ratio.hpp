#ifndef tempest_core_ratio_hpp
#define tempest_core_ratio_hpp

#include <tempest/int.hpp>
#include <tempest/type_traits.hpp>

namespace tempest
{
    namespace detail
    {
        constexpr auto ratio_abs(intmax_t val) noexcept -> intmax_t
        {
            return val < 0 ? -val : val;
        }

        constexpr auto ratio_sign(intmax_t val) noexcept -> intmax_t
        {
            return val < 0 ? -1 : 1;
        }

        constexpr auto ratio_gcd(intmax_t val_a, intmax_t val_b) noexcept -> intmax_t
        {
            val_a = ratio_abs(val_a);
            val_b = ratio_abs(val_b);
            while (val_b != 0)
            {
                auto temp_val = val_b;
                val_b = val_a % val_b;
                val_a = temp_val;
            }
            return val_a;
        }

        template <intmax_t Num, intmax_t Denom>
        struct ratio_canonical
        {
            static_assert(Denom != 0, "ratio denominator cannot be zero");

            static constexpr intmax_t gcd_val = ratio_gcd(Num, Denom);
            static constexpr intmax_t sign_val = ratio_sign(Denom);

            static constexpr intmax_t num = (sign_val * Num) / gcd_val;
            static constexpr intmax_t den = (sign_val * Denom) / gcd_val;
        };
    } // namespace detail

    /// @brief Compile-time rational number representation.
    /// @tparam Num Numerator
    /// @tparam Denom Denominator (defaults to 1)
    template <intmax_t Num, intmax_t Denom = 1>
    struct ratio
    {
        static_assert(Denom != 0, "ratio denominator cannot be zero");

        static constexpr intmax_t num = detail::ratio_canonical<Num, Denom>::num;
        static constexpr intmax_t den = detail::ratio_canonical<Num, Denom>::den;

        using type = ratio<num, den>;
    };

    namespace detail
    {
        template <typename R1, typename R2>
        struct ratio_add_impl
        {
          private:
            static constexpr intmax_t gcd_denom = ratio_gcd(R1::den, R2::den);
            static constexpr intmax_t div_den1 = R1::den / gcd_denom;
            static constexpr intmax_t div_den2 = R2::den / gcd_denom;
            static constexpr intmax_t num_intermediate = (R1::num * div_den2) + (R2::num * div_den1);
            static constexpr intmax_t gcd_intermediate = ratio_gcd(num_intermediate, gcd_denom);

          public:
            using type = ratio<num_intermediate / gcd_intermediate, (R1::den / gcd_intermediate) * div_den2>;
        };

        template <typename R1, typename R2>
        struct ratio_subtract_impl
        {
          private:
            static constexpr intmax_t gcd_denom = ratio_gcd(R1::den, R2::den);
            static constexpr intmax_t div_den1 = R1::den / gcd_denom;
            static constexpr intmax_t div_den2 = R2::den / gcd_denom;
            static constexpr intmax_t num_intermediate = (R1::num * div_den2) - (R2::num * div_den1);
            static constexpr intmax_t gcd_intermediate = ratio_gcd(num_intermediate, gcd_denom);

          public:
            using type = ratio<num_intermediate / gcd_intermediate, (R1::den / gcd_intermediate) * div_den2>;
        };

        template <typename R1, typename R2>
        struct ratio_multiply_impl
        {
          private:
            static constexpr intmax_t gcd1 = ratio_gcd(R1::num, R2::den);
            static constexpr intmax_t gcd2 = ratio_gcd(R2::num, R1::den);

          public:
            using type = ratio<(R1::num / gcd1) * (R2::num / gcd2), (R1::den / gcd2) * (R2::den / gcd1)>;
        };

        template <typename R1, typename R2>
        struct ratio_divide_impl
        {
            static_assert(R2::num != 0, "division by zero ratio");

          public:
            using type = ratio_multiply_impl<R1, ratio<R2::den, R2::num>>::type;
        };
    } // namespace detail

    // Arithmetic ratio operations

    template <typename R1, typename R2>
    using ratio_add = detail::ratio_add_impl<R1, R2>::type;

    template <typename R1, typename R2>
    using ratio_subtract = detail::ratio_subtract_impl<R1, R2>::type;

    template <typename R1, typename R2>
    using ratio_multiply = detail::ratio_multiply_impl<R1, R2>::type;

    template <typename R1, typename R2>
    using ratio_divide = detail::ratio_divide_impl<R1, R2>::type;

    // Relational operations

    template <typename R1, typename R2>
    struct ratio_equal : bool_constant<R1::num == R2::num && R1::den == R2::den>
    {
    };

    template <typename R1, typename R2>
    inline constexpr bool ratio_equal_v = ratio_equal<R1, R2>::value;

    template <typename R1, typename R2>
    struct ratio_not_equal : bool_constant<!ratio_equal_v<R1, R2>>
    {
    };

    template <typename R1, typename R2>
    inline constexpr bool ratio_not_equal_v = ratio_not_equal<R1, R2>::value;

    namespace detail
    {
        template <intmax_t Num1, intmax_t Den1, intmax_t Num2, intmax_t Den2>
        struct ratio_less_cross
        {
            // Both denominators are guaranteed positive by ratio canonicalization.
            // Compare Num1 * Den2 < Num2 * Den1 using double-word if available,
            // or step-wise division comparisons.
            static constexpr bool value = (static_cast<double>(Num1) / static_cast<double>(Den1)) <
                                          (static_cast<double>(Num2) / static_cast<double>(Den2));
        };

        template <typename R1, typename R2>
        struct ratio_less_impl
        {
            static constexpr bool value = []() constexpr -> bool {
                // Different signs
                if constexpr ((R1::num < 0) != (R2::num < 0))
                {
                    return R1::num < R2::num;
                }
                // Same signs or zeros
                // Compute quotient and remainder
                constexpr intmax_t quot1 = R1::num / R1::den;
                constexpr intmax_t quot2 = R2::num / R2::den;
                constexpr intmax_t rem1 = R1::num % R1::den;
                constexpr intmax_t rem2 = R2::num % R2::den;

                if constexpr (quot1 != quot2)
                {
                    return quot1 < quot2;
                }
                else if constexpr (rem1 == 0 || rem2 == 0)
                {
                    return rem1 < rem2;
                }
                else
                {
                    // Recurse with inverted ratios: rem1 / Den1 < rem2 / Den2 <=> Den2 / rem2 < Den1 / rem1
                    return ratio_less_impl<ratio<R2::den, rem2>, ratio<R1::den, rem1>>::value;
                }
            }();
        };
    } // namespace detail

    template <typename R1, typename R2>
    struct ratio_less : bool_constant<detail::ratio_less_impl<R1, R2>::value>
    {
    };

    template <typename R1, typename R2>
    inline constexpr bool ratio_less_v = ratio_less<R1, R2>::value;

    template <typename R1, typename R2>
    struct ratio_less_equal : bool_constant<!ratio_less_v<R2, R1>>
    {
    };

    template <typename R1, typename R2>
    inline constexpr bool ratio_less_equal_v = ratio_less_equal<R1, R2>::value;

    template <typename R1, typename R2>
    struct ratio_greater : bool_constant<ratio_less_v<R2, R1>>
    {
    };

    template <typename R1, typename R2>
    inline constexpr bool ratio_greater_v = ratio_greater<R1, R2>::value;

    template <typename R1, typename R2>
    struct ratio_greater_equal : bool_constant<!ratio_less_v<R1, R2>>
    {
    };

    template <typename R1, typename R2>
    inline constexpr bool ratio_greater_equal_v = ratio_greater_equal<R1, R2>::value;

    namespace detail
    {
        constexpr intmax_t si_deca_factor = 10LL;
        constexpr intmax_t si_hecto_factor = 100LL;
        constexpr intmax_t si_kilo_factor = 1'000LL;
        constexpr intmax_t si_mega_factor = 1'000'000LL;
        constexpr intmax_t si_giga_factor = 1'000'000'000LL;
        constexpr intmax_t si_tera_factor = 1'000'000'000'000LL;
        constexpr intmax_t si_peta_factor = 1'000'000'000'000'000LL;
        constexpr intmax_t si_exa_factor = 1'000'000'000'000'000'000LL;
    } // namespace detail

    // Standard SI prefix ratio type aliases

    using atto = ratio<1, detail::si_exa_factor>;
    using femto = ratio<1, detail::si_peta_factor>;
    using pico = ratio<1, detail::si_tera_factor>;
    using nano = ratio<1, detail::si_giga_factor>;
    using micro = ratio<1, detail::si_mega_factor>;
    using milli = ratio<1, detail::si_kilo_factor>;
    using centi = ratio<1, detail::si_hecto_factor>;
    using deci = ratio<1, detail::si_deca_factor>;
    using deca = ratio<detail::si_deca_factor, 1>;
    using hecto = ratio<detail::si_hecto_factor, 1>;
    using kilo = ratio<detail::si_kilo_factor, 1>;
    using mega = ratio<detail::si_mega_factor, 1>;
    using giga = ratio<detail::si_giga_factor, 1>;
    using tera = ratio<detail::si_tera_factor, 1>;
    using peta = ratio<detail::si_peta_factor, 1>;
    using exa = ratio<detail::si_exa_factor, 1>;
} // namespace tempest

#endif // tempest_core_ratio_hpp
