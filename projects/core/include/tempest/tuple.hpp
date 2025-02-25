#ifndef tempest_core_tuple_hpp
#define tempest_core_tuple_hpp

#include <tempest/functional.hpp>
#include <tempest/meta.hpp>
#include <tempest/type_traits.hpp>
#include <tempest/utility.hpp>

namespace tempest
{
    namespace detail
    {
        template <typename...>
        struct tuple_impl;

        template <>
        struct tuple_impl<>
        {
        };

        template <typename Head, typename... Rest>
        struct tuple_impl<Head, Rest...> : tuple_impl<Rest...>
        {
            using type = Head;
            using base = tuple_impl<Rest...>;

            type value{};

            constexpr tuple_impl() = default;

            constexpr tuple_impl(const type& head, const Rest&... rest)
                : base{tempest::forward<Rest...>(rest)}, value{head}
            {
            }

            template <typename H, typename... R>
            constexpr tuple_impl(H&& head, R&&... rest)
                : base{tempest::forward<R>(rest)...}, value{tempest::forward<H>(head)}
            {
            }

            inline constexpr base& rest() noexcept
            {
                return *this;
            }

            inline constexpr const base& rest() const noexcept
            {
                return *this;
            }

            constexpr void swap(tuple_impl& rhs) noexcept(is_nothrow_swappable_v<type> &&
                                                          (is_nothrow_swappable_v<Rest> && ...))
            {
                swap(value, rhs.value);
                if constexpr (sizeof...(Rest) > 0)
                {
                    rest().swap(rhs.rest());
                }
            }
        };

        template <size_t I, typename... Ts>
        auto& get(tuple_impl<Ts...>& t)
        {
            static_assert(I < sizeof...(Ts), "Index out of bounds.");
            if constexpr (I == 0)
            {
                return t.value;
            }
            else
            {
                return get<I - 1>(t.rest());
            }
        }

        template <size_t I, typename... Ts>
        const auto& get(const tuple_impl<Ts...>& t)
        {
            static_assert(I < sizeof...(Ts), "Index out of bounds.");
            if constexpr (I == 0)
            {
                return t.value;
            }
            else
            {
                return get<I - 1>(t.rest());
            }
        }
    } // namespace detail

    template <typename...>
    class tuple;

    namespace detail
    {
        template <typename T>
        struct unwrap_reference_wrapper
        {
            using type = T;
        };

        template <typename T>
        struct unwrap_reference_wrapper<reference_wrapper<T>>
        {
            using type = T&;
        };

        template <typename T>
        using unwrapped_decay_t = typename unwrap_reference_wrapper<decay_t<T>>::type;
    } // namespace detail

    template <typename... Ts>
    inline constexpr tuple<detail::unwrapped_decay_t<Ts>...> make_tuple(Ts&&... ts)
    {
        return tuple<detail::unwrapped_decay_t<Ts>...>(tempest::forward<Ts>(ts)...);
    }

    template <size_t I, typename... Ts>
    struct tuple_element;

    template <size_t I, typename Head, typename... Rest>
    struct tuple_element<I, tuple<Head, Rest...>> : tuple_element<I - 1, tuple<Rest...>>
    {
    };

    template <typename Head, typename... Rest>
    struct tuple_element<0, tuple<Head, Rest...>>
    {
        using type = Head;
    };

    template <typename... Ts>
    struct tuple_size;

    template <typename... Ts>
    struct tuple_size<tuple<Ts...>> : integral_constant<size_t, sizeof...(Ts)>
    {
    };

    template <typename T>
    inline constexpr size_t tuple_size_v = tuple_size<T>::value;

    namespace detail
    {
        template <typename T, typename U, typename... Ts, typename... Us>
        consteval bool tuple_can_construct_with_helper(core::type_list<T, Ts...>, core::type_list<U, Us...>)
        {
            bool can_construct = is_constructible_v<T, U>;
            if constexpr (sizeof...(Ts) > 0 && sizeof...(Us) > 0)
            {
                can_construct = can_construct && tuple_can_construct_with_helper<Ts..., Us...>(
                                                     tempest::declval<core::type_list<Ts...>>(),
                                                     tempest::declval<core::type_list<Us...>>());
            }

            return can_construct;
        }
    } // namespace detail

    template <typename... Ts>
    class tuple : public detail::tuple_impl<Ts...>
    {
      public:
        constexpr tuple() = default;

        constexpr tuple(const Ts&... ts);

        template <typename... Us>
            requires(sizeof...(Us) == sizeof...(Ts) && sizeof...(Ts) >= 1 &&
                     detail::tuple_can_construct_with_helper<core::type_list<Ts...>, core::type_list<Us...>>())
        constexpr tuple(Us&&... us);

        tuple(const tuple&) = default;
        tuple(tuple&&) noexcept = default;

        ~tuple() = default;

        tuple& operator=(const tuple&) = default;
        tuple& operator=(tuple&&) noexcept = default;
    };

    template <typename... Ts>
    inline constexpr tuple<Ts...>::tuple(const Ts&... ts)
        : detail::tuple_impl<Ts...>{tempest::forward<const Ts>(ts)...}
    {
    }

    template <typename... Ts>
    template <typename... Us>
        requires(sizeof...(Us) == sizeof...(Ts) && sizeof...(Ts) >= 1 &&
                 detail::tuple_can_construct_with_helper<core::type_list<Ts...>, core::type_list<Us...>>())
    inline constexpr tuple<Ts...>::tuple(Us&&... us) : detail::tuple_impl<Ts...>{tempest::forward<Us>(us)...}
    {
    }

    template <typename... Ts>
    tuple<Ts&...> tie(Ts&... ts) noexcept
    {
        return tuple<Ts&...>(ts...);
    }

    template <typename... Ts>
    tuple<Ts&&...> forward_as_tuple(Ts&&... ts) noexcept
    {
        return tuple<Ts&&...>(tempest::forward<Ts>(ts)...);
    }

    template <size_t I, typename... Ts>
    inline constexpr typename tuple_element<I, tuple<Ts...>>::type& get(tuple<Ts...>& t) noexcept
    {
        return detail::get<I>(t);
    }

    template <size_t I, typename... Ts>
    inline constexpr const typename tuple_element<I, tuple<Ts...>>::type& get(const tuple<Ts...>& t) noexcept
    {
        return detail::get<I>(t);
    }

    template <size_t I, typename... Ts>
    inline constexpr typename tuple_element<I, tuple<Ts...>>::type&& get(tuple<Ts...>&& t) noexcept
    {
        return tempest::move(detail::get<I>(t));
    }

    template <size_t I, typename... Ts>
    inline constexpr const typename tuple_element<I, tuple<Ts...>>::type&& get(const tuple<Ts...>&& t) noexcept
    {
        return tempest::move(detail::get<I>(t));
    }

    namespace detail
    {
        // Template metafunction to test if a variadic template parameter pack contains any duplicate types.
        template <typename... Ts>
        struct has_duplicate_types;

        template <typename T, typename... Ts>
        struct has_duplicate_types<T, Ts...> : disjunction<is_same<T, Ts>..., has_duplicate_types<Ts...>>
        {
        };

        template <>
        struct has_duplicate_types<> : false_type
        {
        };

        template <typename... Ts>
        concept all_different = !has_duplicate_types<Ts...>::value;

        // Template metafunction to get the index of a type in a variadic template parameter pack.
        template <typename T, typename... Ts>
        struct index_of_type;

        template <typename T, typename... Ts>
        struct index_of_type<T, T, Ts...> : integral_constant<size_t, 0>
        {
        };

        template <typename T, typename U, typename... Ts>
        struct index_of_type<T, U, Ts...> : integral_constant<size_t, 1 + index_of_type<T, Ts...>::value>
        {
        };

        template <typename T, typename... Ts>
        inline constexpr size_t index_of_type_v = index_of_type<T, Ts...>::value;
    } // namespace detail

    template <typename T, typename... Ts>
    inline constexpr T& get(tuple<Ts...>& t) noexcept
    {
        static_assert(detail::all_different<Ts...>, "Duplicate types in tuple.");
        static_assert(detail::index_of_type_v<T, Ts...> < sizeof...(Ts), "Type not found in tuple.");
        return get<detail::index_of_type_v<T, Ts...>>(t);
    }

    template <typename T, typename... Ts>
    inline constexpr const T& get(const tuple<Ts...>& t) noexcept
    {
        static_assert(detail::all_different<Ts...>, "Duplicate types in tuple.");
        static_assert(detail::index_of_type_v<T, Ts...> < sizeof...(Ts), "Type not found in tuple.");
        return get<detail::index_of_type_v<T, Ts...>>(t);
    }

    template <typename T, typename... Ts>
    inline constexpr T&& get(tuple<Ts...>&& t) noexcept
    {
        static_assert(detail::all_different<Ts...>, "Duplicate types in tuple.");
        static_assert(detail::index_of_type_v<T, Ts...> < sizeof...(Ts), "Type not found in tuple.");
        return get<detail::index_of_type_v<T, Ts...>>(tempest::move(t));
    }

    template <typename T, typename... Ts>
    inline constexpr const T&& get(const tuple<Ts...>&& t) noexcept
    {
        static_assert(detail::all_different<Ts...>, "Duplicate types in tuple.");
        static_assert(detail::index_of_type_v<T, Ts...> < sizeof...(Ts), "Type not found in tuple.");
        return get<detail::index_of_type_v<T, Ts...>>(tempest::move(t));
    }

    namespace detail
    {
        template <typename Fn, typename Tuple, size_t... Is>
        constexpr decltype(auto) apply_impl(Fn&& fn, Tuple&& tuple, index_sequence<Is...>)
        {
            return tempest::invoke(tempest::forward<Fn>(fn), get<Is>(tempest::forward<Tuple>(tuple))...);
        }
    } // namespace detail

    template <typename Fn, typename Tuple>
    constexpr decltype(auto) apply(Fn&& fn, Tuple&& tuple)
    {
        return detail::apply_impl(tempest::forward<Fn>(fn), tempest::forward<Tuple>(tuple),
                                  tempest::make_index_sequence<tuple_size_v<decay_t<Tuple>>>{});
    }
} // namespace tempest

// Add tuple_size and tuple_element specializations for tuple in the std namespace.
namespace std
{
    template <typename... Ts>
    struct tuple_size<tempest::tuple<Ts...>> : tempest::tuple_size<tempest::tuple<Ts...>>
    {
    };

    template <size_t I, typename... Ts>
    struct tuple_element<I, tempest::tuple<Ts...>>
    {
        using type = typename tempest::tuple_element<I, tempest::tuple<Ts...>>::type;
    };
} // namespace std

#endif // tempest_core_tuple_hpp