#ifndef tempest_core_compare_hpp
#define tempest_core_compare_hpp

#include <tempest/api.hpp>
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
            consteval unspec(unspec* /*unused*/) noexcept
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

        friend constexpr auto operator==(partial_ordering, partial_ordering) noexcept -> bool = default;
        friend constexpr auto operator==(partial_ordering /*p*/, detail::unspec /*unused*/) noexcept -> bool;
        friend constexpr auto operator<(partial_ordering /*p*/, detail::unspec /*unused*/) noexcept -> bool;
        friend constexpr auto operator>(partial_ordering /*p*/, detail::unspec /*unused*/) noexcept -> bool;
        friend constexpr auto operator<=(partial_ordering /*p*/, detail::unspec /*unused*/) noexcept -> bool;
        friend constexpr auto operator>=(partial_ordering /*p*/, detail::unspec /*unused*/) noexcept -> bool;
        friend constexpr auto operator<(partial_ordering /*p*/, partial_ordering /*q*/) noexcept -> bool;
        friend constexpr auto operator>(partial_ordering /*p*/, partial_ordering /*q*/) noexcept -> bool;
        friend constexpr auto operator<=(partial_ordering /*p*/, partial_ordering /*q*/) noexcept -> bool;
        friend constexpr auto operator>=(partial_ordering /*p*/, partial_ordering /*q*/) noexcept -> bool;

        friend constexpr auto operator<=>(partial_ordering /*p*/, detail::unspec /*unused*/) noexcept -> partial_ordering;
        friend constexpr auto operator<=>(detail::unspec /*unused*/, partial_ordering /*p*/) noexcept -> partial_ordering;

      private:
        friend class strong_ordering;
        friend class weak_ordering;

        comparison_categories::type _value;
    };

    constexpr partial_ordering::partial_ordering(comparison_categories::ordering o) noexcept
        : _value{to_underlying(o)}
    {
    }

    constexpr partial_ordering::partial_ordering(comparison_categories::no_order o) noexcept
        : _value{to_underlying(o)}
    {
    }

    constexpr auto operator==(partial_ordering p, detail::unspec /*unused*/) noexcept -> bool
    {
        return p._value == 0;
    }

    constexpr auto operator<(partial_ordering p, detail::unspec /*unused*/) noexcept -> bool
    {
        return p._value < 0;
    }

    constexpr auto operator>(partial_ordering p, detail::unspec /*unused*/) noexcept -> bool
    {
        return p._value > 0;
    }

    constexpr auto operator<=(partial_ordering p, detail::unspec /*unused*/) noexcept -> bool
    {
        return p._value <= 0;
    }

    constexpr auto operator>=(partial_ordering p, detail::unspec /*unused*/) noexcept -> bool
    {
        return p._value >= 0;
    }

    constexpr auto operator<(partial_ordering p, partial_ordering q) noexcept -> bool
    {
        return p._value < q._value;
    }

    constexpr auto operator>(partial_ordering p, partial_ordering q) noexcept -> bool
    {
        return p._value > q._value;
    }

    constexpr auto operator<=(partial_ordering p, partial_ordering q) noexcept -> bool
    {
        return p._value <= q._value;
    }

    constexpr auto operator>=(partial_ordering p, partial_ordering q) noexcept -> bool
    {
        return p._value >= q._value;
    }

    constexpr auto operator<=>(partial_ordering p, detail::unspec /*unused*/) noexcept -> partial_ordering
    {
        return p;
    }

    constexpr auto operator<=>(detail::unspec /*unused*/, partial_ordering p) noexcept -> partial_ordering
    {
        if ((p._value & 1) != 0)
        {
            return partial_ordering(static_cast<comparison_categories::ordering>(-p._value));
        }
        
        
            return p;
       
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

        friend constexpr auto operator==(weak_ordering, weak_ordering) noexcept -> bool = default;
        friend constexpr auto operator==(weak_ordering /*w*/, detail::unspec /*unused*/) noexcept -> bool;
        friend constexpr auto operator<(weak_ordering /*w*/, detail::unspec /*unused*/) noexcept -> bool;
        friend constexpr auto operator>(weak_ordering /*w*/, detail::unspec /*unused*/) noexcept -> bool;
        friend constexpr auto operator<=(weak_ordering /*w*/, detail::unspec /*unused*/) noexcept -> bool;
        friend constexpr auto operator>=(weak_ordering /*w*/, detail::unspec /*unused*/) noexcept -> bool;
        friend constexpr auto operator<(weak_ordering /*w*/, weak_ordering /*v*/) noexcept -> bool;
        friend constexpr auto operator>(weak_ordering /*w*/, weak_ordering /*v*/) noexcept -> bool;
        friend constexpr auto operator<=(weak_ordering /*w*/, weak_ordering /*v*/) noexcept -> bool;
        friend constexpr auto operator>=(weak_ordering /*w*/, weak_ordering /*v*/) noexcept -> bool;
        friend constexpr auto operator<=>(weak_ordering /*w*/, detail::unspec /*unused*/) noexcept -> weak_ordering;
        friend constexpr auto operator<=>(detail::unspec /*unused*/, weak_ordering /*w*/) noexcept -> weak_ordering;

      private:
        friend class strong_ordering;

        comparison_categories::type _value;
    };

    constexpr weak_ordering::weak_ordering(comparison_categories::ordering o) noexcept : _value{to_underlying(o)}
    {
    }

    constexpr weak_ordering::operator partial_ordering() const noexcept
    {
        return partial_ordering(static_cast<comparison_categories::ordering>(_value));
    }

    constexpr auto operator==(weak_ordering w, detail::unspec /*unused*/) noexcept -> bool
    {
        return w._value == 0;
    }

    constexpr auto operator<(weak_ordering w, detail::unspec /*unused*/) noexcept -> bool
    {
        return w._value < 0;
    }

    constexpr auto operator>(weak_ordering w, detail::unspec /*unused*/) noexcept -> bool
    {
        return w._value > 0;
    }

    constexpr auto operator<=(weak_ordering w, detail::unspec /*unused*/) noexcept -> bool
    {
        return w._value <= 0;
    }

    constexpr auto operator>=(weak_ordering w, detail::unspec /*unused*/) noexcept -> bool
    {
        return w._value >= 0;
    }

    constexpr auto operator<(weak_ordering w, weak_ordering v) noexcept -> bool
    {
        return w._value < v._value;
    }

    constexpr auto operator>(weak_ordering w, weak_ordering v) noexcept -> bool
    {
        return w._value > v._value;
    }

    constexpr auto operator<=(weak_ordering w, weak_ordering v) noexcept -> bool
    {
        return w._value <= v._value;
    }

    constexpr auto operator>=(weak_ordering w, weak_ordering v) noexcept -> bool
    {
        return w._value >= v._value;
    }

    constexpr auto operator<=>(weak_ordering w, detail::unspec /*unused*/) noexcept -> weak_ordering
    {
        return w;
    }

    constexpr auto operator<=>(detail::unspec /*unused*/, weak_ordering w) noexcept -> weak_ordering
    {
        if ((w._value & 1) != 0)
        {
            return weak_ordering(static_cast<comparison_categories::ordering>(-w._value));
        }
        
        
            return w;
       
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

        friend constexpr auto operator==(strong_ordering, strong_ordering) noexcept -> bool = default;
        friend constexpr auto operator==(strong_ordering /*s*/, detail::unspec /*unused*/) noexcept -> bool;
        friend constexpr auto operator<(strong_ordering /*s*/, detail::unspec /*unused*/) noexcept -> bool;
        friend constexpr auto operator>(strong_ordering /*s*/, detail::unspec /*unused*/) noexcept -> bool;
        friend constexpr auto operator<=(strong_ordering /*s*/, detail::unspec /*unused*/) noexcept -> bool;
        friend constexpr auto operator>=(strong_ordering /*s*/, detail::unspec /*unused*/) noexcept -> bool;
        friend constexpr auto operator<(strong_ordering /*s*/, strong_ordering /*t*/) noexcept -> bool;
        friend constexpr auto operator>(strong_ordering /*s*/, strong_ordering /*t*/) noexcept -> bool;
        friend constexpr auto operator<=(strong_ordering /*s*/, strong_ordering /*t*/) noexcept -> bool;
        friend constexpr auto operator>=(strong_ordering /*s*/, strong_ordering /*t*/) noexcept -> bool;
        friend constexpr auto operator<=>(strong_ordering /*s*/, detail::unspec /*unused*/) noexcept -> strong_ordering;
        friend constexpr auto operator<=>(detail::unspec /*unused*/, strong_ordering /*s*/) noexcept -> strong_ordering;

      private:
        comparison_categories::type _value;
    };

    constexpr strong_ordering::strong_ordering(comparison_categories::ordering o) noexcept
        : _value{to_underlying(o)}
    {
    }

    constexpr strong_ordering::operator weak_ordering() const noexcept
    {
        return weak_ordering(static_cast<comparison_categories::ordering>(_value));
    }

    constexpr strong_ordering::operator partial_ordering() const noexcept
    {
        return partial_ordering(static_cast<comparison_categories::ordering>(_value));
    }

    constexpr auto operator==(strong_ordering s, detail::unspec /*unused*/) noexcept -> bool
    {
        return s._value == 0;
    }

    constexpr auto operator<(strong_ordering s, detail::unspec /*unused*/) noexcept -> bool
    {
        return s._value < 0;
    }

    constexpr auto operator>(strong_ordering s, detail::unspec /*unused*/) noexcept -> bool
    {
        return s._value > 0;
    }

    constexpr auto operator<=(strong_ordering s, detail::unspec /*unused*/) noexcept -> bool
    {
        return s._value <= 0;
    }

    constexpr auto operator>=(strong_ordering s, detail::unspec /*unused*/) noexcept -> bool
    {
        return s._value >= 0;
    }

    constexpr auto operator<(strong_ordering s, strong_ordering t) noexcept -> bool
    {
        return s._value < t._value;
    }

    constexpr auto operator>(strong_ordering s, strong_ordering t) noexcept -> bool
    {
        return s._value > t._value;
    }

    constexpr auto operator<=(strong_ordering s, strong_ordering t) noexcept -> bool
    {
        return s._value <= t._value;
    }

    constexpr auto operator>=(strong_ordering s, strong_ordering t) noexcept -> bool
    {
        return s._value >= t._value;
    }

    constexpr auto operator<=>(strong_ordering s, detail::unspec /*unused*/) noexcept -> strong_ordering
    {
        return s;
    }

    constexpr auto operator<=>(detail::unspec /*unused*/, strong_ordering s) noexcept -> strong_ordering
    {
        if ((s._value & 1) != 0)
        {
            return strong_ordering(static_cast<comparison_categories::ordering>(-s._value));
        }
        
        
            return s;
       
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
    using common_comparison_category_t = common_comparison_category<Ts...>::type;

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

        static constexpr auto compare(T t, U u) noexcept -> strong_ordering
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
                    if (t > u)
                    {
                        return strong_ordering::greater;
                    }
                    
                    
                        return strong_ordering::equal;
                   
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
                
                
                    return three_way_comparer<make_unsigned_t<t_base>, u_base>::compare(
                        static_cast<make_unsigned_t<t_base>>(t), u);
               
            }
            else
            {
                // If only U is signed, check u for negative.
                if (u < 0)
                {
                    return strong_ordering::greater;
                }
                
                
                    return three_way_comparer<t_base, make_unsigned_t<u_base>>::compare(
                        t, static_cast<make_unsigned_t<u_base>>(u));
               
            }
        }
    };

    template <typename T, typename U>
        requires is_pointer_v<remove_cvref_t<T>> && is_pointer_v<remove_cvref_t<U>>
    struct three_way_comparer<T, U>
    {
        static constexpr auto compare(T t, U u) noexcept -> strong_ordering
        {
            if (t < u)
            {
                return strong_ordering::less;
            }
            if (t > u)
            {
                return strong_ordering::greater;
            }
            
            
                return strong_ordering::equal;
           
        }
    };

    template <floating_point T, floating_point U>
    struct three_way_comparer<T, U>
    {
        static constexpr auto compare(T t, U u) noexcept -> strong_ordering
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
                    if (t > u)
                    {
                        return strong_ordering::greater;
                    }
                    
                    
                        return strong_ordering::equal;
                   
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

    constexpr auto is_eq(partial_ordering cmp) noexcept -> bool
    {
        return cmp == partial_ordering::equivalent;
    }

    constexpr auto is_neq(partial_ordering cmp) noexcept -> bool
    {
        return cmp != partial_ordering::equivalent;
    }

    constexpr auto is_lt(partial_ordering cmp) noexcept -> bool
    {
        return cmp == partial_ordering::less;
    }

    constexpr auto is_lteq(partial_ordering cmp) noexcept -> bool
    {
        return cmp == partial_ordering::less || cmp == partial_ordering::equivalent;
    }

    constexpr auto is_gt(partial_ordering cmp) noexcept -> bool
    {
        return cmp == partial_ordering::greater;
    }

    constexpr auto is_gteq(partial_ordering cmp) noexcept -> bool
    {
        return cmp == partial_ordering::greater || cmp == partial_ordering::equivalent;
    }
} // namespace tempest

#endif // tempest_core_compare_hpp
