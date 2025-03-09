#ifndef tempest_core_compare_hpp
#define tempest_core_compare_hpp

#include <tempest/concepts.hpp>
#include <tempest/forward.hpp>
#include <tempest/int.hpp>
#include <tempest/math.hpp>
#include <tempest/to_underlying.hpp>
#include <tempest/type_traits.hpp>

namespace tempest
{
    namespace comparison_categories
    {
        using type = int8_t;

        enum class ordering : type
        {
            less = -1,
            equal = 0,
            greater = 1,
        };

        enum class no_order : type
        {
            unordered = 2,
        };
    } // namespace comparison_categories

    namespace detail
    {
        struct unspec
        {
            consteval unspec(unspec*) noexcept
            {
            }
        };
    } // namespace detail

    class strong_ordering;
    class weak_ordering;
    class partial_ordering;

    class partial_ordering
    {
        constexpr explicit partial_ordering(comparison_categories::ordering o) noexcept;
        constexpr partial_ordering(comparison_categories::no_order o) noexcept;

      public:
        static const partial_ordering less;
        static const partial_ordering equivalent;
        static const partial_ordering greater;
        static const partial_ordering unordered;

        friend constexpr bool operator==(partial_ordering, partial_ordering) noexcept = default;
        friend constexpr bool operator==(partial_ordering, detail::unspec) noexcept;
        friend constexpr bool operator<(partial_ordering, detail::unspec) noexcept;
        friend constexpr bool operator>(partial_ordering, detail::unspec) noexcept;
        friend constexpr bool operator<=(partial_ordering, detail::unspec) noexcept;
        friend constexpr bool operator>=(partial_ordering, detail::unspec) noexcept;
        friend constexpr bool operator<(partial_ordering, partial_ordering) noexcept;
        friend constexpr bool operator>(partial_ordering, partial_ordering) noexcept;
        friend constexpr bool operator<=(partial_ordering, partial_ordering) noexcept;
        friend constexpr bool operator>=(partial_ordering, partial_ordering) noexcept;

        friend constexpr partial_ordering operator<=>(partial_ordering, detail::unspec) noexcept;
        friend constexpr partial_ordering operator<=>(detail::unspec, partial_ordering) noexcept;

      private:
        friend class strong_ordering;
        friend class weak_ordering;

        comparison_categories::type _value;
    };

    inline constexpr partial_ordering::partial_ordering(comparison_categories::ordering o) noexcept
        : _value{to_underlying(o)}
    {
    }

    inline constexpr partial_ordering::partial_ordering(comparison_categories::no_order o) noexcept
        : _value{to_underlying(o)}
    {
    }

    inline constexpr bool operator==(partial_ordering p, detail::unspec) noexcept
    {
        return p._value == 0;
    }

    inline constexpr bool operator<(partial_ordering p, detail::unspec) noexcept
    {
        return p._value < 0;
    }

    inline constexpr bool operator>(partial_ordering p, detail::unspec) noexcept
    {
        return p._value > 0;
    }

    inline constexpr bool operator<=(partial_ordering p, detail::unspec) noexcept
    {
        return p._value <= 0;
    }

    inline constexpr bool operator>=(partial_ordering p, detail::unspec) noexcept
    {
        return p._value >= 0;
    }

    inline constexpr bool operator<(partial_ordering p, partial_ordering q) noexcept
    {
        return p._value < q._value;
    }

    inline constexpr bool operator>(partial_ordering p, partial_ordering q) noexcept
    {
        return p._value > q._value;
    }

    inline constexpr bool operator<=(partial_ordering p, partial_ordering q) noexcept
    {
        return p._value <= q._value;
    }

    inline constexpr bool operator>=(partial_ordering p, partial_ordering q) noexcept
    {
        return p._value >= q._value;
    }

    inline constexpr partial_ordering operator<=>(partial_ordering p, detail::unspec) noexcept
    {
        return p;
    }

    inline constexpr partial_ordering operator<=>(detail::unspec, partial_ordering p) noexcept
    {
        if (p._value & 1)
        {
            return partial_ordering(static_cast<comparison_categories::ordering>(-p._value));
        }
        else
        {
            return p;
        }
    }

    inline constexpr partial_ordering partial_ordering::less{comparison_categories::ordering::less};
    inline constexpr partial_ordering partial_ordering::equivalent{comparison_categories::ordering::equal};
    inline constexpr partial_ordering partial_ordering::greater{comparison_categories::ordering::greater};
    inline constexpr partial_ordering partial_ordering::unordered{comparison_categories::no_order::unordered};

    class weak_ordering
    {
        constexpr explicit weak_ordering(comparison_categories::ordering o) noexcept;

      public:
        static const weak_ordering less;
        static const weak_ordering equivalent;
        static const weak_ordering greater;

        constexpr operator partial_ordering() const noexcept;

        friend constexpr bool operator==(weak_ordering, weak_ordering) noexcept = default;
        friend constexpr bool operator==(weak_ordering, detail::unspec) noexcept;
        friend constexpr bool operator<(weak_ordering, detail::unspec) noexcept;
        friend constexpr bool operator>(weak_ordering, detail::unspec) noexcept;
        friend constexpr bool operator<=(weak_ordering, detail::unspec) noexcept;
        friend constexpr bool operator>=(weak_ordering, detail::unspec) noexcept;
        friend constexpr bool operator<(weak_ordering, weak_ordering) noexcept;
        friend constexpr bool operator>(weak_ordering, weak_ordering) noexcept;
        friend constexpr bool operator<=(weak_ordering, weak_ordering) noexcept;
        friend constexpr bool operator>=(weak_ordering, weak_ordering) noexcept;
        friend constexpr weak_ordering operator<=>(weak_ordering, detail::unspec) noexcept;
        friend constexpr weak_ordering operator<=>(detail::unspec, weak_ordering) noexcept;

      private:
        friend class strong_ordering;

        comparison_categories::type _value;
    };

    inline constexpr weak_ordering::weak_ordering(comparison_categories::ordering o) noexcept : _value{to_underlying(o)}
    {
    }

    inline constexpr weak_ordering::operator partial_ordering() const noexcept
    {
        return partial_ordering(static_cast<comparison_categories::ordering>(_value));
    }

    inline constexpr bool operator==(weak_ordering w, detail::unspec) noexcept
    {
        return w._value == 0;
    }

    inline constexpr bool operator<(weak_ordering w, detail::unspec) noexcept
    {
        return w._value < 0;
    }

    inline constexpr bool operator>(weak_ordering w, detail::unspec) noexcept
    {
        return w._value > 0;
    }

    inline constexpr bool operator<=(weak_ordering w, detail::unspec) noexcept
    {
        return w._value <= 0;
    }

    inline constexpr bool operator>=(weak_ordering w, detail::unspec) noexcept
    {
        return w._value >= 0;
    }

    inline constexpr bool operator<(weak_ordering w, weak_ordering v) noexcept
    {
        return w._value < v._value;
    }

    inline constexpr bool operator>(weak_ordering w, weak_ordering v) noexcept
    {
        return w._value > v._value;
    }

    inline constexpr bool operator<=(weak_ordering w, weak_ordering v) noexcept
    {
        return w._value <= v._value;
    }

    inline constexpr bool operator>=(weak_ordering w, weak_ordering v) noexcept
    {
        return w._value >= v._value;
    }

    inline constexpr weak_ordering operator<=>(weak_ordering w, detail::unspec) noexcept
    {
        return w;
    }

    inline constexpr weak_ordering operator<=>(detail::unspec, weak_ordering w) noexcept
    {
        if (w._value & 1)
        {
            return weak_ordering(static_cast<comparison_categories::ordering>(-w._value));
        }
        else
        {
            return w;
        }
    }

    inline constexpr weak_ordering weak_ordering::less{comparison_categories::ordering::less};
    inline constexpr weak_ordering weak_ordering::equivalent{comparison_categories::ordering::equal};
    inline constexpr weak_ordering weak_ordering::greater{comparison_categories::ordering::greater};

    class strong_ordering
    {
        constexpr explicit strong_ordering(comparison_categories::ordering o) noexcept;

      public:
        static const strong_ordering less;
        static const strong_ordering equal;
        static const strong_ordering equivalent;
        static const strong_ordering greater;

        constexpr operator weak_ordering() const noexcept;
        constexpr operator partial_ordering() const noexcept;

        friend constexpr bool operator==(strong_ordering, strong_ordering) noexcept = default;
        friend constexpr bool operator==(strong_ordering, detail::unspec) noexcept;
        friend constexpr bool operator<(strong_ordering, detail::unspec) noexcept;
        friend constexpr bool operator>(strong_ordering, detail::unspec) noexcept;
        friend constexpr bool operator<=(strong_ordering, detail::unspec) noexcept;
        friend constexpr bool operator>=(strong_ordering, detail::unspec) noexcept;
        friend constexpr bool operator<(strong_ordering, strong_ordering) noexcept;
        friend constexpr bool operator>(strong_ordering, strong_ordering) noexcept;
        friend constexpr bool operator<=(strong_ordering, strong_ordering) noexcept;
        friend constexpr bool operator>=(strong_ordering, strong_ordering) noexcept;
        friend constexpr strong_ordering operator<=>(strong_ordering, detail::unspec) noexcept;
        friend constexpr strong_ordering operator<=>(detail::unspec, strong_ordering) noexcept;

      private:
        comparison_categories::type _value;
    };

    inline constexpr strong_ordering::strong_ordering(comparison_categories::ordering o) noexcept
        : _value{to_underlying(o)}
    {
    }

    inline constexpr strong_ordering::operator weak_ordering() const noexcept
    {
        return weak_ordering(static_cast<comparison_categories::ordering>(_value));
    }

    inline constexpr strong_ordering::operator partial_ordering() const noexcept
    {
        return partial_ordering(static_cast<comparison_categories::ordering>(_value));
    }

    inline constexpr bool operator==(strong_ordering s, detail::unspec) noexcept
    {
        return s._value == 0;
    }

    inline constexpr bool operator<(strong_ordering s, detail::unspec) noexcept
    {
        return s._value < 0;
    }

    inline constexpr bool operator>(strong_ordering s, detail::unspec) noexcept
    {
        return s._value > 0;
    }

    inline constexpr bool operator<=(strong_ordering s, detail::unspec) noexcept
    {
        return s._value <= 0;
    }

    inline constexpr bool operator>=(strong_ordering s, detail::unspec) noexcept
    {
        return s._value >= 0;
    }

    inline constexpr bool operator<(strong_ordering s, strong_ordering t) noexcept
    {
        return s._value < t._value;
    }

    inline constexpr bool operator>(strong_ordering s, strong_ordering t) noexcept
    {
        return s._value > t._value;
    }

    inline constexpr bool operator<=(strong_ordering s, strong_ordering t) noexcept
    {
        return s._value <= t._value;
    }

    inline constexpr bool operator>=(strong_ordering s, strong_ordering t) noexcept
    {
        return s._value >= t._value;
    }

    inline constexpr strong_ordering operator<=>(strong_ordering s, detail::unspec) noexcept
    {
        return s;
    }

    inline constexpr strong_ordering operator<=>(detail::unspec, strong_ordering s) noexcept
    {
        if (s._value & 1)
        {
            return strong_ordering(static_cast<comparison_categories::ordering>(-s._value));
        }
        else
        {
            return s;
        }
    }

    inline constexpr strong_ordering strong_ordering::less{comparison_categories::ordering::less};
    inline constexpr strong_ordering strong_ordering::equal{comparison_categories::ordering::equal};
    inline constexpr strong_ordering strong_ordering::equivalent{comparison_categories::ordering::equal};
    inline constexpr strong_ordering strong_ordering::greater{comparison_categories::ordering::greater};

    namespace detail
    {
        template <typename T>
        inline constexpr uint32_t comparison_category_id = 1;

        template <>
        inline constexpr uint32_t comparison_category_id<partial_ordering> = 2;

        template <>
        inline constexpr uint32_t comparison_category_id<weak_ordering> = 4;

        template <>
        inline constexpr uint32_t comparison_category_id<strong_ordering> = 8;

        template <typename... Ts>
        constexpr auto common_comparison_category()
        {
            constexpr uint32_t categories = (comparison_category_id<Ts> | ...);

            // If a non-category type is present, return void.
            if constexpr (categories & 1)
            {
                return;
            }
            else if constexpr (static_cast<bool>(categories & comparison_category_id<partial_ordering>))
            {
                return partial_ordering::equivalent;
            }
            else if constexpr (static_cast<bool>(categories & comparison_category_id<weak_ordering>))
            {
                return weak_ordering::equivalent;
            }
            else
            {
                return strong_ordering::equivalent;
            }
        }
    } // namespace detail

    template <typename... Ts>
    struct common_comparison_category
    {
        using type = decltype(detail::common_comparison_category<Ts...>());
    };

    template <typename... Ts>
    using common_comparison_category_t = typename common_comparison_category<Ts...>::type;

    template <typename T, typename U = T>
    using compare_three_way_result_t =
        decltype(declval<const remove_reference_t<T>&>() <=> declval<const remove_reference_t<U>&>());

    template <typename T, typename U = T>
    struct compare_three_way_result
    {
    };

    template <typename T, typename U>
        requires requires { typename compare_three_way_result_t<T, U>; }
    struct compare_three_way_result<T, U>
    {
        using type = compare_three_way_result_t<T, U>;
    };

    namespace detail
    {
        template <typename T, typename C>
        concept compare_as = same_as<common_comparison_category_t<T, C>, C>;
    } // namespace detail

    template <typename T, typename C = partial_ordering>
    concept three_way_comparable = detail::half_equality_comparable<T, T> && detail::half_ordered<T, T> &&
                                   requires(const remove_reference_t<T>& x, const remove_reference_t<T>& y) {
                                       { x <=> y } -> detail::compare_as<C>;
                                   };

    template <typename T1, typename T2, typename C = partial_ordering>
    concept three_way_comparable_wtih =
        three_way_comparable<T1, C> && three_way_comparable<T2, C> &&
        common_reference_with<const remove_reference_t<T1>&, const remove_reference_t<T2>&> &&
        three_way_comparable<common_reference_t<const remove_reference_t<T1>&, const remove_reference_t<T2>&>, C> &&
        detail::weakly_eq_cmp_with<T1, T2> && partially_ordered_with<T1, T2> &&
        requires(const remove_reference_t<T1>& x, const remove_reference_t<T2>& y) {
            { x <=> y } -> detail::compare_as<C>;
            { y <=> x } -> detail::compare_as<C>;
        };

    template <typename T, typename U = T>
    struct three_way_comparer;

    template <typename T, typename U>
        requires integral<remove_cvref_t<T>> && integral<remove_cvref_t<U>>
    struct three_way_comparer<T, U>
    {
        using t_base = remove_cvref_t<T>;
        using u_base = remove_cvref_t<U>;

        static constexpr strong_ordering compare(T t, U u) noexcept
        {
            // If the same signedness
            // - If both types are the same size, compare directly.
            // - If not, compare from the wider type to the narrower type.
            // If different signedness
            // - If the argument with the signed type is negative, return the appropriate ordering.
            // - If both are non-negative, compare as unsigned.

            if constexpr (signed_integral<t_base> == signed_integral<u_base>)
            {
                if constexpr (sizeof(t_base) == sizeof(u_base))
                {
                    if (t < u)
                    {
                        return strong_ordering::less;
                    }
                    else if (t > u)
                    {
                        return strong_ordering::greater;
                    }
                    else
                    {
                        return strong_ordering::equal;
                    }
                }
                else if constexpr (sizeof(t_base) > sizeof(u_base))
                {
                    // promote U to T
                    return three_way_comparer<t_base, t_base>::compare(t, static_cast<t_base>(u));
                }
                else
                {
                    // promote T to U
                    return three_way_comparer<u_base, u_base>::compare(static_cast<u_base>(t), u);
                }
            }
            else if constexpr (signed_integral<t_base>)
            {
                // If only T is signed, check t for negative.
                if (t < 0)
                {
                    return strong_ordering::less;
                }
                else
                {
                    return three_way_comparer<make_unsigned_t<t_base>, u_base>::compare(
                        static_cast<make_unsigned_t<t_base>>(t), u);
                }
            }
            else
            {
                // If only U is signed, check u for negative.
                if (u < 0)
                {
                    return strong_ordering::greater;
                }
                else
                {
                    return three_way_comparer<t_base, make_unsigned_t<u_base>>::compare(
                        t, static_cast<make_unsigned_t<u_base>>(u));
                }
            }
        }
    };

    template <typename T, typename U>
        requires is_pointer_v<remove_cvref_t<T>> && is_pointer_v<remove_cvref_t<U>>
    struct three_way_comparer<T, U>
    {
        static constexpr strong_ordering compare(T t, U u) noexcept
        {
            if (t < u)
            {
                return strong_ordering::less;
            }
            else if (t > u)
            {
                return strong_ordering::greater;
            }
            else
            {
                return strong_ordering::equal;
            }
        }
    };

    template <floating_point T, floating_point U>
    struct three_way_comparer<T, U>
    {
        static constexpr strong_ordering compare(T t, U u) noexcept
        {
            // If the types are the same, compare directly.
            // Else promote the narrower type to the wider type and compare.

            if constexpr (same_as<T, U>)
            {
                bool tnan = isnan(t);
                bool unan = isnan(u);

                if (!tnan && !unan)
                {
                    if (t < u)
                    {
                        return strong_ordering::less;
                    }
                    else if (t > u)
                    {
                        return strong_ordering::greater;
                    }
                    else
                    {
                        return strong_ordering::equal;
                    }
                }

                bool tsign = signbit(t);
                bool usign = signbit(u);

                if (tnan == unan && tsign == usign)
                {
                    return strong_ordering::equal;
                }

                // If t is negative NaN and u is not negative NaN, t < u
                bool tnegnan = tnan && tsign;
                bool unotnegnan = !unan || (unan && usign);

                if (tnegnan && unotnegnan)
                {
                    return strong_ordering::less;
                }

                // If t is not positive NaN and u is positive NaN, t < u
                bool tnotposnan = !tnan || (tnan && !tsign);
                bool uposnan = unan && !usign;

                if (tnotposnan && uposnan)
                {
                    return strong_ordering::less;
                }

                // Both are NaN, return equal
                return strong_ordering::equal;
            }
            else if constexpr (sizeof(T) > sizeof(U))
            {
                // promote U to T
                return three_way_comparer<T, T>::compare(t, static_cast<T>(u));
            }
            else
            {
                // promote T to U
                return three_way_comparer<U, U>::compare(static_cast<U>(t), u);
            }
        }
    };

    template <typename T1, typename T2>
    struct three_way_comparer
    {
        static constexpr auto compare(T1 t, T2 u) noexcept
        {
            return t <=> u;
        }
    };

    struct compare_three_way
    {
        template <typename T, typename U>
        constexpr auto operator()(T&& t, U&& u) const
        {
            return three_way_comparer<T, U>::compare(tempest::forward<T>(t), tempest::forward<U>(u));
        }

        using is_transparent = void;
    };

    inline constexpr bool is_eq(partial_ordering cmp) noexcept
    {
        return cmp == partial_ordering::equivalent;
    }

    inline constexpr bool is_neq(partial_ordering cmp) noexcept
    {
        return cmp != partial_ordering::equivalent;
    }

    inline constexpr bool is_lt(partial_ordering cmp) noexcept
    {
        return cmp == partial_ordering::less;
    }

    inline constexpr bool is_lteq(partial_ordering cmp) noexcept
    {
        return cmp == partial_ordering::less || cmp == partial_ordering::equivalent;
    }

    inline constexpr bool is_gt(partial_ordering cmp) noexcept
    {
        return cmp == partial_ordering::greater;
    }

    inline constexpr bool is_gteq(partial_ordering cmp) noexcept
    {
        return cmp == partial_ordering::greater || cmp == partial_ordering::equivalent;
    }
} // namespace tempest

#endif // tempest_core_compare_hpp
