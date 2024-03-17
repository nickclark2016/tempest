#ifndef tempest_core_meta_hpp
#define tempest_core_meta_hpp

#include <algorithm>
#include <array>
#include <cstddef>
#include <source_location>
#include <string>
#include <string_view>
#include <type_traits>

namespace tempest::core
{
    template <typename T>
    constexpr auto get_type_name() noexcept
    {
        auto src = std::source_location::current();
        std::string_view here = src.function_name();

#if defined(_MSC_VER)
        std::string_view prefix = "auto __cdecl tempest::core::get_type_name<";
        std::string_view suffix = ">(void) noexcept";
#elif defined(__clang__)
        std::string_view prefix = "auto tempest::core::get_type_name() [T = ";
        std::string_view suffix = "]";
#elif defined(__GNUC__)
        std::string_view prefix = "constexpr auto tempest::core::get_type_name() [ with T = ";
        std::string_view suffix = "]";
#else
#error Unsupported compiler
#endif
        here.remove_prefix(prefix.size());
        here.remove_suffix(suffix.size());

        if (here.starts_with("enum "))
        {
            here.remove_prefix(5);
        }

        if (here.starts_with("struct "))
        {
            here.remove_prefix(7);
        }

        if (here.starts_with("class "))
        {
            here.remove_prefix(6);
        }

        return here;
    }

    template <std::size_t N>
    struct string_literal
    {
        static constexpr std::size_t size = N;

        constexpr string_literal(const char (&str)[N])
        {
            std::copy_n(str, N, value);
        }

        char value[N];
    };

    template <string_literal Name>
    struct named_type
    {
        static constexpr std::array<char, sizeof(Name.value)> value = std::to_array(Name.value);
    };

    struct named_type_comparator
    {
        template <typename T1, typename T2>
        static constexpr bool compare() noexcept
        {
            return T1::value < T2::value;
        }
    };

    struct unnamed_type_comparator
    {
        template <typename T1, typename T2>
        static constexpr bool compare() noexcept
        {
            return get_type_name<T1>() < get_type_name<T2>();
        }
    };

    template <typename... Ts>
    struct type_list;

    template <template <typename...> typename T, typename...>
    struct instantiate;

    template <template <typename...> typename T, typename... Ts>
    struct instantiate<T, type_list<Ts...>>
    {
        using type = T<Ts...>;
    };

    template <template <typename...> typename T, typename... Ts>
    using instantiate_t = typename instantiate<T, Ts...>::type;

    template <typename...>
    struct type_list_concat;

    template <typename... Ts, typename... Us>
    struct type_list_concat<type_list<Ts...>, type_list<Us...>>
    {
        using type = type_list<Ts..., Us...>;
    };

    template <typename... Ts>
    using type_list_concat_t = typename type_list_concat<Ts...>::type;

    template <std::size_t N, typename... Ts>
    struct take_type;

    template <std::size_t N, typename... Ts>
    using take_type_t = typename take_type<N, Ts...>::type;

    template <typename... Ts>
    struct take_type<0, type_list<Ts...>>
    {
        using type = type_list<>;
        using rest_type = type_list<Ts...>;
    };

    template <typename T, typename... Ts>
    struct take_type<1, type_list<T, Ts...>>
    {
        using type = type_list<T>;
        using rest_type = type_list<Ts...>;
    };

    template <std::size_t N, typename T, typename... Ts>
    struct take_type<N, type_list<T, Ts...>>
    {
        using type = type_list_concat_t<type_list<T>, take_type_t<N - 1, type_list<Ts...>>>;
        using rest_type = typename take_type<N - 1, type_list<Ts...>>::rest_type;
    };

    template <typename C, typename... Ts>
    struct sorted_type_list;

    template <typename C, typename... Ts>
    using sorted_type_list_t = sorted_type_list<C, Ts...>::type;

    template <typename C, typename T>
    struct sorted_type_list<C, type_list<T>>
    {
        using type = type_list<T>;
    };

    template <typename C, typename T1, typename T2>
    struct sorted_type_list<C, type_list<T1, T2>>
    {
        using type = std::conditional_t<C::template compare<T1, T2>(), type_list<T1, T2>, type_list<T2, T1>>;
    };

    namespace detail
    {
        template <typename C, typename...>
        struct type_merge_sort;

        template <typename C, typename... Ts>
        using type_merge_sort_t = typename type_merge_sort<C, Ts...>::type;

        template <typename C, typename... Ts>
        struct type_merge_sort<C, type_list<>, type_list<Ts...>>
        {
            using type = type_list<Ts...>;
        };

        template <typename C, typename... Ts>
        struct type_merge_sort<C, type_list<Ts...>, type_list<>>
        {
            using type = type_list<Ts...>;
        };

        template <typename C, typename THead, typename... Ts, typename UHead, typename... Us>
        struct type_merge_sort<C, type_list<THead, Ts...>, type_list<UHead, Us...>>
        {
            using type = std::conditional_t<
                C::template compare<THead, UHead>(),
                type_list_concat_t<type_list<THead>, type_merge_sort_t<C, type_list<Ts...>, type_list<UHead, Us...>>>,
                type_list_concat_t<type_list<UHead>, type_merge_sort_t<C, type_list<THead, Ts...>, type_list<Us...>>>>;
        };
    } // namespace detail

    template <typename C, typename... Ts>
    struct sorted_type_list<C, type_list<Ts...>>
    {
        static constexpr auto left_size = sizeof...(Ts) / 2;
        using split = take_type<left_size, type_list<Ts...>>;
        using type = detail::type_merge_sort_t<C, sorted_type_list_t<C, typename split::type>,
                                               sorted_type_list_t<C, typename split::rest_type>>;
    };

    namespace detail
    {
        struct type_index final
        {
            [[nodiscard]] static std::size_t next() noexcept
            {
                static std::size_t value{0};
                return value++;
            }
        };

        // FN1VA Hash - https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function
        template <typename>
        struct fnv1a_traits;

        template <>
        struct fnv1a_traits<std::uint32_t>
        {
            using type = std::uint32_t;
            static constexpr std::uint32_t offset = 2166136261;
            static constexpr std::uint32_t prime = 16777619;
        };

        template <>
        struct fnv1a_traits<std::uint64_t>
        {
            using type = std::uint64_t;
            static constexpr std::uint64_t offset = 14695981039346656037ull;
            static constexpr std::uint64_t prime = 1099511628211ull;
        };

        template <typename C>
        struct hash_string_base
        {
            using value_type = C;
            using size_type = std::size_t;
            using hash_type = std::size_t;

            const value_type* c_string;
            size_type length;
            hash_type hash;
        };

        template <typename C>
        class basic_hash_string : hash_string_base<C>
        {
            using base_type = hash_string_base<C>;
            using hash_traits_type = fnv1a_traits<typename base_type::hash_type>;

            struct const_str_wrapper
            {
                constexpr const_str_wrapper(const C* str) noexcept : c_string(str)
                {
                }

                const C* c_string;
            };

            [[nodiscard]] static constexpr auto hash(const C* str) noexcept
            {
                hash_string_base<C> base{str, 0, hash_traits_type::offset};

                while (str[base.length] != 0)
                {
                    base.hash =
                        (base.hash ^ static_cast<hash_traits_type::type>(str[base.length])) * hash_traits_type::prime;

                    base.length++;
                }

                return base;
            }

            [[nodiscard]] static constexpr auto hash(const C* str, std::size_t len) noexcept
            {
                hash_string_base<C> base{str, 0, hash_traits_type::offset};

                for (size_type pos{}; pos < len; ++pos)
                {
                    base.hash = (base.hash ^ static_cast<hash_traits_type::type>(str[pos])) * hash_traits_type::prime;

                    base.length++;
                }

                return base;
            }

          public:
            using value_type = typename base_type::value_type;
            using size_type = typename base_type::size_type;
            using hash_type = typename base_type::hash_type;

            [[nodiscard]] static constexpr hash_type from(const value_type* str, size_type sz) noexcept
            {
                return basic_hash_string{str, sz};
            }

            template <std::size_t N>
            [[nodiscard]] static constexpr hash_type from(const value_type (&str)[N]) noexcept
            {
                return basic_hash_string{str};
            }

            [[nodiscard]] static constexpr hash_type value(const_str_wrapper wrapper) noexcept
            {
                return basic_hash_string{wrapper};
            }

            constexpr basic_hash_string() noexcept : base_type{}
            {
            }

            constexpr basic_hash_string(const value_type* str, size_type len) noexcept : base_type{hash(str, len)}
            {
            }

            template <std::size_t N>
            constexpr basic_hash_string(const value_type (&str)[N]) noexcept : base_type{hash(str)}
            {
            }

            explicit constexpr basic_hash_string(const_str_wrapper wrapper) noexcept : base_type{hash(wrapper.c_string)}
            {
            }

            constexpr size_type size() const noexcept
            {
                return base_type::length;
            }

            constexpr const value_type* data() const noexcept
            {
                return base_type::c_string;
            }

            constexpr operator const value_type*() const noexcept
            {
                return base_type::c_string;
            }

            constexpr hash_type value() const noexcept
            {
                return base_type::hash;
            }

            constexpr operator hash_type() const noexcept
            {
                return base_type::hash;
            }
        };

        template <typename C>
        [[nodiscard]] constexpr bool operator==(const basic_hash_string<C>& lhs,
                                                const basic_hash_string<C>& rhs) noexcept
        {
            return lhs.value() == rhs.value();
        }

        template <typename C>
        [[nodiscard]] constexpr bool operator!=(const basic_hash_string<C>& lhs,
                                                const basic_hash_string<C>& rhs) noexcept
        {
            return lhs.value() != rhs.value();
        }

        template <typename C>
        [[nodiscard]] constexpr bool operator<(const basic_hash_string<C>& lhs,
                                               const basic_hash_string<C>& rhs) noexcept
        {
            return lhs.value() < rhs.value();
        }

        template <typename C>
        [[nodiscard]] constexpr bool operator<=(const basic_hash_string<C>& lhs,
                                                const basic_hash_string<C>& rhs) noexcept
        {
            return lhs.value() <= rhs.value();
        }

        template <typename C>
        [[nodiscard]] constexpr bool operator>=(const basic_hash_string<C>& lhs,
                                                const basic_hash_string<C>& rhs) noexcept
        {
            return lhs.value() >= rhs.value();
        }

        template <typename C>
        [[nodiscard]] constexpr bool operator>(const basic_hash_string<C>& lhs,
                                               const basic_hash_string<C>& rhs) noexcept
        {
            return lhs.value() > rhs.value();
        }

        using hash_string = basic_hash_string<char>;

        template <typename T>
        [[nodiscard]] constexpr std::size_t get_type_hash() noexcept
        {
            std::string_view type_name = get_type_name<T>();
            return hash_string::from(type_name.data(), type_name.size());
        }
    } // namespace detail

    template <typename T, typename = void>
    struct type_index final
    {
        using id_type = std::size_t;

        [[nodiscard]] static id_type value() noexcept
        {
            static const id_type id = detail::type_index::next();
            return id;
        }
    };

    template <typename T, typename = void>
    struct type_hash final
    {
        static constexpr std::size_t value() noexcept
        {
            constexpr auto v = detail::get_type_hash<T>();
            return v;
        }
    };

    template <typename T, typename = void>
    struct type_name final
    {
        static constexpr std::string_view value() noexcept
        {
            return get_type_name<T>();
        }
    };

    class type_info final
    {
      public:
        template <typename T>
        constexpr type_info(std::in_place_type_t<T>) noexcept
            : _id{type_index<std::remove_cv_t<std::remove_reference_t<T>>>::value()},
              _hash{type_hash<std::remove_cv_t<std::remove_reference_t<T>>>::value()},
              _name{type_name<std::remove_cv_t<std::remove_reference_t<T>>>::value()}
        {
        }

        auto index() const noexcept
        {
            return _id;
        }

        auto hash() const noexcept
        {
            return _hash;
        }

        auto name() const noexcept
        {
            return _name;
        }

      private:
        std::size_t _id;
        std::size_t _hash;
        std::string_view _name;
    };

    template <typename T>
    [[nodiscard]] const type_info& type_id() noexcept
    {
        using base = std::remove_cv_t<std::remove_reference_t<T>>;
        if constexpr (std::is_same_v<base, T>)
        {
            static type_info instance{std::in_place_type_t<T>{}};
            return instance;
        }
        return type_id<base>();
    }

    template <std::size_t N>
    struct select_t : select_t<N - 1>
    {
    };

    template <>
    struct select_t<0>
    {
    };

    template <std::size_t N>
    inline constexpr select_t<N> select;

    template <typename T, typename = void>
    struct size_of : std::integral_constant<std::size_t, 0u>
    {
    };

    template <typename T>
    struct size_of<T, std::void_t<decltype(sizeof(T))>> : std::integral_constant<std::size_t, sizeof(T)>
    {
    };

    template <typename T>
    inline constexpr std::size_t size_of_v = size_of<T>::value;
} // namespace tempest::core

#endif // tempest_core_meta_hpp