#ifndef tempest_core_tuple_hpp
#define tempest_core_tuple_hpp

#include <tempest/api.hpp>
#include <tempest/functional.hpp>
#include <tempest/meta.hpp>
#include <tempest/type_traits.hpp>
#include <tempest/utility.hpp>

namespace tempest
{
    template <typename... Ts>
    class tuple;

    namespace detail
    {
        struct exact_args_tag
        {
            constexpr explicit exact_args_tag() noexcept = default;
        };

        template <typename...>
        struct tuple_impl;

        template <typename... Ts>
        struct tuple_impl_size;

        template <typename... Ts>
        struct tuple_impl_size<tuple_impl<Ts...>> : integral_constant<size_t, sizeof...(Ts)>
        {
        };

        template <bool Same, typename Dst, typename... Src>
        inline constexpr bool tuple_can_construct_with = false;

        template <typename... Dst, typename... Src>
        inline constexpr bool tuple_can_construct_with<true, tuple_impl<Dst...>, Src...> =
            conjunction_v<is_constructible<Dst, Src>...>;

        template <typename Dst, typename... Src>
        inline constexpr bool tuple_constructible_v =
            tuple_can_construct_with<tuple_impl_size<Dst>::value == sizeof...(Src), Dst, Src...>;

        template <typename Dst, typename... Src>
        struct tuple_constructible : bool_constant<tuple_constructible_v<Dst, Src...>>
        {
        };

        // Tuple perfect forwarding constructor constraints
        template <typename S, typename T, typename... Rest>
        struct tuple_perfect_forwarding : true_type
        {
        };

        template <typename S, typename T>
        struct tuple_perfect_forwarding<S, T> : bool_constant<!is_same_v<S, remove_cvref_t<T>>>
        {
        };

        template <typename T>
        struct tuple_val
        {
            constexpr tuple_val() : val{} {};

            template <typename U>
            constexpr tuple_val(U&& u) : val{tempest::forward<U>(u)} {};

            T val;
        };

        template <typename T>
        struct tuple_val<T&>
        {
            constexpr tuple_val(T& u) : val{u} {};

            // Reference wrapper
            template <typename U>
            constexpr tuple_val(const reference_wrapper<U>&& u) : val{u.get()} {};

            T& val;
        };

        template <>
        struct tuple_impl<>
        {
            constexpr tuple_impl() noexcept = default;
            constexpr tuple_impl(const tuple_impl&) = default;

            template <typename Tag>
            constexpr tuple_impl(Tag /*unused*/) noexcept
                requires(is_same_v<Tag, exact_args_tag>)
            {
            }

            constexpr auto operator=(const tuple_impl&) -> tuple_impl& = default;
        };

        template <typename Head, typename... Rest>
        struct tuple_impl<Head, Rest...> : tuple_impl<Rest...>
        {
            using type = Head;
            using base = tuple_impl<Rest...>;

            tuple_val<type> value;

            constexpr tuple_impl() = default;

            template <typename Tag, typename H, typename... R>
            constexpr tuple_impl(Tag /*unused*/, H&& head, R&&... rest)
                requires(is_same_v<Tag, exact_args_tag>)
                : base{exact_args_tag{}, tempest::forward_like<Rest>(rest)...}, value{tempest::forward_like<Head>(head)}
            {
            }

            template <typename type2 = type>
            constexpr tuple_impl(const type& head, const Rest&... rest)
                requires(tuple_constructible_v<tuple_impl, const type2&, const Rest&...>)
                : tuple_impl(exact_args_tag{}, tempest::forward_like<Rest>(rest)...), value{head}
            {
            }

            template <typename H, typename... R>
            constexpr tuple_impl(H&& head, R&&... rest)
                requires(conjunction_v<tuple_perfect_forwarding<tuple_impl, H, Rest...>,
                                       tuple_constructible<tuple_impl, H, Rest...>>)
                : tuple_impl(exact_args_tag{}, tempest::forward_like<H>(head), tempest::forward_like<R>(rest)...)
            {
            }

            constexpr auto rest() noexcept -> base&
            {
                return *this;
            }

            [[nodiscard]] constexpr auto rest() const noexcept -> const base&
            {
                return *this;
            }

            constexpr void swap(tuple_impl& rhs) noexcept(is_nothrow_swappable_v<type> &&
                                                          (is_nothrow_swappable_v<Rest> && ...))
                requires(is_swappable_v<Head> && ... && is_swappable_v<Rest>)
            {
                using tempest::swap;

                swap(value.val, rhs.value.val);
                if constexpr (sizeof...(Rest) > 0)
                {
                    rest().swap(rhs.rest());
                }
            }
        };

        template <bool, size_t I>
        struct tuple_get_helper
        {
          public:
            template <typename... Ts>
            static auto get(tuple_impl<Ts...>& t) noexcept -> auto&
            {
                static_assert(I < sizeof...(Ts), "Index out of bounds.");
                if constexpr (I == 0)
                {
                    return t.value.val;
                }
                else
                {
                    return tuple_get_helper<false, I - 1>::get(t.rest());
                }
            }

            template <typename... Ts>
            static auto get(tuple_impl<Ts...>&& t) noexcept -> auto&&
            {
                static_assert(I < sizeof...(Ts), "Index out of bounds.");
                if constexpr (I == 0)
                {
                    return tempest::move(t.value.val);
                }
                else
                {
                    return tuple_get_helper<false, I - 1>::get(tempest::move(t.rest()));
                }
            }
        };

        template <size_t I>
        struct tuple_get_helper<true, I>
        {
          public:
            template <typename... Ts>
            static auto get(const tuple_impl<Ts...>& t) noexcept -> const auto&
            {
                static_assert(I < sizeof...(Ts), "Index out of bounds.");
                if constexpr (I == 0)
                {
                    return t.value.val;
                }
                else
                {
                    return tuple_get_helper<true, I - 1>::get(t.rest());
                }
            }

            template <typename... Ts>
            static auto get(const tuple_impl<Ts...>&& t) noexcept -> const auto&&
            {
                static_assert(I < sizeof...(Ts), "Index out of bounds.");
                if constexpr (I == 0)
                {
                    return tempest::move(t.value.val);
                }
                else
                {
                    return tuple_get_helper<true, I - 1>::get(tempest::move(t.rest()));
                }
            }
        };
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
        using unwrapped_decay_t = unwrap_reference_wrapper<decay_t<T>>::type;
    } // namespace detail

    template <typename... Ts>
    constexpr auto make_tuple(Ts&&... ts) -> tuple<detail::unwrapped_decay_t<Ts>...>
    {
        using res = tuple<detail::unwrapped_decay_t<Ts>...>;

        return res(tempest::forward_like<Ts>(ts)...);
    }

    template <size_t I, typename... Ts>
    struct tuple_element;

    template <size_t I, typename Head, typename... Rest>
    struct tuple_element<I, tuple<Head, Rest...>> : tuple_element<I - 1, tuple<Rest...>>
    {
    };

    template <size_t I, typename Head, typename... Rest>
    struct tuple_element<I, const tuple<Head, Rest...>> : tuple_element<I - 1, const tuple<Rest...>>
    {
    };

    template <typename Head, typename... Rest>
    struct tuple_element<0, tuple<Head, Rest...>>
    {
        using type = Head;
    };

    template <typename Head, typename... Rest>
    struct tuple_element<0, const tuple<Head, Rest...>>
    {
        using type = const Head;
    };

    template <typename... Ts>
    struct tuple_size;

    template <typename... Ts>
    struct tuple_size<tuple<Ts...>> : integral_constant<size_t, sizeof...(Ts)>
    {
    };

    template <typename... Ts>
    struct tuple_size<const tuple<Ts...>> : integral_constant<size_t, sizeof...(Ts)>
    {
    };

    template <typename T>
    inline constexpr size_t tuple_size_v = tuple_size<T>::value;

    template <typename... Ts>
    class tuple : public detail::tuple_impl<Ts...>
    {
      public:
        constexpr tuple() = default;

        constexpr tuple(const Ts&... ts);

        template <typename... Us>
        constexpr tuple(Us&&... us);

        tuple(const tuple&) = default;
        tuple(tuple&&) noexcept = default;

        ~tuple() = default;

        auto operator=(const tuple&) -> tuple& = default;
        auto operator=(tuple&&) noexcept -> tuple& = default;
    };

    template <typename... Ts>
    constexpr tuple<Ts...>::tuple(const Ts&... ts) : detail::tuple_impl<Ts...>{tempest::forward<const Ts>(ts)...}
    {
    }

    template <typename... Ts>
    template <typename... Us>
    constexpr tuple<Ts...>::tuple(Us&&... us) : detail::tuple_impl<Ts...>{tempest::forward_like<Us>(us)...}
    {
    }

    template <typename... Ts>
    auto tie(Ts&... ts) noexcept -> tuple<Ts&...>
    {
        return tuple<Ts&...>(ts...);
    }

    template <typename... Ts>
    auto forward_as_tuple(Ts&&... ts) noexcept -> tuple<Ts&&...>
    {
        return tuple<Ts&&...>(tempest::forward<Ts>(ts)...);
    }

    template <size_t I, typename... Ts>
    constexpr auto get(tuple<Ts...>& t) noexcept -> decltype(auto)
    {
        return detail::tuple_get_helper<false, I>::get(t);
    }

    template <size_t I, typename... Ts>
    constexpr auto get(const tuple<Ts...>& t) noexcept -> decltype(auto)
    {
        return detail::tuple_get_helper<true, I>::get(t);
    }

    template <size_t I, typename... Ts>
    constexpr auto get(tuple<Ts...>&& t) noexcept -> decltype(auto)
    {
        return detail::tuple_get_helper<false, I>::get(t);
    }

    template <size_t I, typename... Ts>
    constexpr auto get(const tuple<Ts...>&& t) noexcept -> decltype(auto)
    {
        return detail::tuple_get_helper<true, I>::get(t);
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
    constexpr auto get(tuple<Ts...>& t) noexcept -> T&
    {
        static_assert(detail::all_different<Ts...>, "Duplicate types in tuple.");
        static_assert(detail::index_of_type_v<T, Ts...> < sizeof...(Ts), "Type not found in tuple.");
        return get<detail::index_of_type_v<T, Ts...>>(t);
    }

    template <typename T, typename... Ts>
    constexpr auto get(const tuple<Ts...>& t) noexcept -> const T&
    {
        static_assert(detail::all_different<Ts...>, "Duplicate types in tuple.");
        static_assert(detail::index_of_type_v<T, Ts...> < sizeof...(Ts), "Type not found in tuple.");
        return get<detail::index_of_type_v<T, Ts...>>(t);
    }

    template <typename T, typename... Ts>
    constexpr auto get(tuple<Ts...>&& t) noexcept -> T&&
    {
        static_assert(detail::all_different<Ts...>, "Duplicate types in tuple.");
        static_assert(detail::index_of_type_v<T, Ts...> < sizeof...(Ts), "Type not found in tuple.");
        return get<detail::index_of_type_v<T, Ts...>>(tempest::move(t));
    }

    template <typename T, typename... Ts>
    constexpr auto get(const tuple<Ts...>&& t) noexcept -> const T&&
    {
        static_assert(detail::all_different<Ts...>, "Duplicate types in tuple.");
        static_assert(detail::index_of_type_v<T, Ts...> < sizeof...(Ts), "Type not found in tuple.");
        return get<detail::index_of_type_v<T, Ts...>>(tempest::move(t));
    }

    namespace detail
    {
        template <typename Fn, typename Tuple, size_t... Is>
        constexpr auto apply_impl(Fn&& fn, Tuple&& tuple, index_sequence<Is...> /*unused*/) -> decltype(auto)
        {
            return tempest::invoke(tempest::forward<Fn>(fn), get<Is>(tempest::forward<Tuple>(tuple))...);
        }
    } // namespace detail

    template <typename Fn, typename Tuple>
    constexpr auto apply(Fn&& fn, Tuple&& tuple) -> decltype(auto)
    {
        return detail::apply_impl(tempest::forward<Fn>(fn), tempest::forward<Tuple>(tuple),
                                  tempest::make_index_sequence<tuple_size_v<decay_t<Tuple>>>{});
    }
} // namespace tempest

// Add tuple_size and tuple_element specializations for tuple in the std namespace.
namespace std
{
    template <typename T>
    struct tuple_size;

    template <size_t I, typename T>
    struct tuple_element;

    template <typename... Ts>
    struct tuple_size<tempest::tuple<Ts...>> : tempest::tuple_size<tempest::tuple<Ts...>>
    {
    };

    template <typename... Ts>
    struct tuple_size<const tempest::tuple<Ts...>> : tempest::tuple_size<const tempest::tuple<Ts...>>
    {
    };

    template <size_t I, typename... Ts>
    struct tuple_element<I, tempest::tuple<Ts...>>
    {
        using type = tempest::tuple_element<I, tempest::tuple<Ts...>>::type;
    };

    template <size_t I, typename... Ts>
    struct tuple_element<I, const tempest::tuple<Ts...>>
    {
        using type = tempest::tuple_element<I, const tempest::tuple<Ts...>>::type;
    };
} // namespace std

#endif // tempest_core_tuple_hpp