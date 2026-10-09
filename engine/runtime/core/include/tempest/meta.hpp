#ifndef tempest_core_meta_hpp
#define tempest_core_meta_hpp

#include <tempest/algorithm.hpp>
#include <tempest/api.hpp>
#include <tempest/array.hpp>
#include <tempest/int.hpp>
#include <tempest/source_location.hpp>
#include <tempest/string_view.hpp>
#include <tempest/type_traits.hpp>

namespace tempest::core
{
    namespace detail
    {
        constexpr auto find_substring(string_view haystack, string_view needle) noexcept -> const char*
        {
            if (needle.empty())
            {
                return haystack.begin();
            }
            if (needle.size() > haystack.size())
            {
                return haystack.end();
            }
            const auto search_end = haystack.end() - needle.size() + 1;
            for (auto current = haystack.begin(); current != search_end; ++current)
            {
                if (tempest::equal(needle.begin(), needle.end(), current))
                {
                    return current;
                }
            }
            return haystack.end();
        }

        constexpr auto contains_substring(string_view haystack, string_view needle) noexcept -> bool
        {
            return find_substring(haystack, needle) != haystack.end();
        }

        template <typename T>
        constexpr auto raw_type_name() noexcept -> string_view
        {
            auto src = tempest::source_location::current();
            string_view here = src.function_name();

#if defined(_MSC_VER) && !defined(__clang__)
            constexpr auto marker = string_view{"raw_type_name<"};
            const auto marker_position = find_substring(here, marker);
            if (marker_position != here.end())
            {
                const auto start_pos = marker_position + marker.size();
                constexpr auto suffix = string_view{">(void) noexcept"};
                if (ends_with(here, suffix))
                {
                    here = string_view{start_pos, here.end() - suffix.size()};
                }
            }
#elif defined(__clang__) || defined(__GNUC__)
            constexpr auto marker = string_view{"[T = "};
            constexpr auto alt_marker = string_view{"[ with T = "};
            auto marker_position = find_substring(here, marker);
            auto start_pos = here.end();
            if (marker_position != here.end())
            {
                start_pos = marker_position + marker.size();
            }
            else
            {
                marker_position = find_substring(here, alt_marker);
                if (marker_position != here.end())
                {
                    start_pos = marker_position + alt_marker.size();
                }
            }

            if (marker_position != here.end())
            {
                if (ends_with(here, ']'))
                {
                    here = string_view{start_pos, here.end() - 1};
                }
            }
#else
#error "Unsupported compiler"
#endif

            return here;
        }

        template <size_t Capacity>
        struct fixed_string_buffer
        {
            static constexpr size_t default_length = 0;

            using value_type = char;
            using size_type = size_t;
            using difference_type = ptrdiff_t;
            using reference = char&;
            using const_reference = const char&;
            using pointer = char*;
            using const_pointer = const char*;
            using iterator = pointer;
            using const_iterator = const_pointer;

            array<char, Capacity> characters{};
            size_t length = default_length;

            [[nodiscard]] constexpr auto begin() noexcept -> iterator
            {
                return characters.data();
            }

            [[nodiscard]] constexpr auto begin() const noexcept -> const_iterator
            {
                return characters.data();
            }

            [[nodiscard]] constexpr auto cbegin() const noexcept -> const_iterator
            {
                return characters.data();
            }

            [[nodiscard]] constexpr auto end() noexcept -> iterator
            {
                return characters.data() + length;
            }

            [[nodiscard]] constexpr auto end() const noexcept -> const_iterator
            {
                return characters.data() + length;
            }

            [[nodiscard]] constexpr auto cend() const noexcept -> const_iterator
            {
                return characters.data() + length;
            }

            [[nodiscard]] constexpr auto size() const noexcept -> size_type
            {
                return length;
            }

            [[nodiscard]] constexpr auto empty() const noexcept -> bool
            {
                return length == 0;
            }

            [[nodiscard]] constexpr auto back() noexcept -> reference
            {
                return characters[length - 1];
            }

            [[nodiscard]] constexpr auto back() const noexcept -> const_reference
            {
                return characters[length - 1];
            }

            [[nodiscard]] constexpr auto data() noexcept -> pointer
            {
                return characters.data();
            }

            [[nodiscard]] constexpr auto data() const noexcept -> const_pointer
            {
                return characters.data();
            }

            [[nodiscard]] constexpr auto capacity() const noexcept -> size_type
            {
                return Capacity;
            }

            constexpr void push_back(char character) noexcept
            {
                if (length < Capacity - 1)
                {
                    characters[length] = character;
                    ++length;
                    characters[length] = '\0';
                }
            }

            constexpr void pop_back() noexcept
            {
                if (length > 0)
                {
                    --length;
                    characters[length] = '\0';
                }
            }

            constexpr void append(string_view text) noexcept
            {
                for (const auto character : text)
                {
                    push_back(character);
                }
            }

            [[nodiscard]] constexpr auto view() const noexcept -> string_view
            {
                return string_view{characters.data(), length};
            }
        };

        static constexpr size_t default_normalized_name_capacity = 512;

        struct type_token_replacement
        {
            string_view pattern;
            string_view replacement;
        };

        static constexpr array<type_token_replacement, 3> type_token_replacements = {
            type_token_replacement{string_view{"unsigned __int64"}, string_view{"unsigned long long"}},
            type_token_replacement{string_view{"signed __int64"}, string_view{"long long"}},
            type_token_replacement{string_view{"__int64"}, string_view{"long long"}},
        };

        static constexpr array<string_view, 2> compiler_modifiers = {
            string_view{"__cdecl"},
            string_view{"__ptr64"},
        };

        static constexpr array<string_view, 4> elaborated_type_keywords = {
            string_view{"struct"},
            string_view{"class"},
            string_view{"union"},
            string_view{"enum"},
        };

        constexpr auto is_identifier_character(char character) noexcept -> bool
        {
            return (character >= 'a' && character <= 'z') ||
                   (character >= 'A' && character <= 'Z') ||
                   (character >= '0' && character <= '9') ||
                   (character == '_');
        }

        constexpr auto is_unsupported_type_scope(string_view raw_name) noexcept -> bool
        {
            constexpr auto function_scope_marker = string_view{"()::"};
            if (contains_substring(raw_name, function_scope_marker))
            {
                return true;
            }

            constexpr auto lambda_marker = string_view{"lambda"};
            if (contains_substring(raw_name, lambda_marker))
            {
                return true;
            }

            constexpr auto anonymous_marker = string_view{"anonymous"};
            constexpr auto braced_anonymous_marker = string_view{"{anonymous}"};
            if (contains_substring(raw_name, anonymous_marker) ||
                contains_substring(raw_name, braced_anonymous_marker))
            {
                return true;
            }

            return false;
        }

        constexpr auto is_word_boundary(const char* begin, const char* current) noexcept -> bool
        {
            return (current == begin) || !is_identifier_character(*(current - 1));
        }

        constexpr auto skip_spaces(const char* current, const char* end) noexcept -> const char*
        {
            while (current != end && *current == ' ')
            {
                ++current;
            }
            return current;
        }

        template <typename Buffer>
        constexpr auto try_consume_type_replacement(const char* current, const char* end, Buffer& out) noexcept
            -> const char*
        {
            for (const auto& rule : type_token_replacements)
            {
                const auto pattern_length = rule.pattern.size();
                if (static_cast<size_t>(end - current) >= pattern_length &&
                    tempest::equal(rule.pattern.begin(), rule.pattern.end(), current))
                {
                    const auto next_position = current + pattern_length;
                    if (next_position == end || !is_identifier_character(*next_position))
                    {
                        out.append(rule.replacement);
                        return next_position;
                    }
                }
            }
            return nullptr;
        }

        constexpr auto try_consume_modifier(const char* current, const char* end) noexcept -> const char*
        {
            for (const auto modifier : compiler_modifiers)
            {
                const auto modifier_length = modifier.size();
                if (static_cast<size_t>(end - current) >= modifier_length &&
                    tempest::equal(modifier.begin(), modifier.end(), current))
                {
                    const auto next_position = current + modifier_length;
                    if (next_position == end || !is_identifier_character(*next_position))
                    {
                        return skip_spaces(next_position, end);
                    }
                }
            }
            return nullptr;
        }

        constexpr auto try_consume_elaborated_keyword(const char* current, const char* end) noexcept -> const char*
        {
            for (const auto keyword : elaborated_type_keywords)
            {
                const auto keyword_length = keyword.size();
                if (static_cast<size_t>(end - current) > keyword_length &&
                    tempest::equal(keyword.begin(), keyword.end(), current))
                {
                    const auto after_keyword = current + keyword_length;
                    if (*after_keyword == ' ')
                    {
                        return skip_spaces(after_keyword, end);
                    }
                }
            }
            return nullptr;
        }

        constexpr auto is_space_removable_before(char character) noexcept -> bool
        {
            return character == '>' || character == '<' || character == ',' ||
                   character == '*' || character == '&';
        }

        template <typename Buffer>
        constexpr auto process_whitespace(Buffer& out, const char* current, const char* end) noexcept -> const char*
        {
            const auto next_non_space = skip_spaces(current, end);
            if (next_non_space == end)
            {
                return end;
            }

            if (out.empty() || out.back() == ' ' || out.back() == ',' || out.back() == '<')
            {
                return next_non_space;
            }

            if (is_space_removable_before(*next_non_space))
            {
                return next_non_space;
            }

            out.push_back(' ');
            return next_non_space;
        }

        template <typename Buffer>
        constexpr auto trim_trailing_spaces(Buffer& buffer) noexcept -> void
        {
            while (!buffer.empty() && buffer.back() == ' ')
            {
                buffer.pop_back();
            }
        }

        constexpr auto normalize_type_string(string_view raw_name) noexcept
            -> fixed_string_buffer<default_normalized_name_capacity>
        {
            auto result = fixed_string_buffer<default_normalized_name_capacity>{};
            const auto begin = raw_name.begin();
            const auto end = raw_name.end();
            auto current = begin;

            while (current != end)
            {
                if (is_word_boundary(begin, current))
                {
                    if (const auto next_position = try_consume_type_replacement(current, end, result);
                        next_position != nullptr)
                    {
                        current = next_position;
                        continue;
                    }

                    if (const auto next_position = try_consume_modifier(current, end); next_position != nullptr)
                    {
                        current = next_position;
                        continue;
                    }

                    if (const auto next_position = try_consume_elaborated_keyword(current, end);
                        next_position != nullptr)
                    {
                        current = next_position;
                        continue;
                    }
                }

                if (*current == ' ')
                {
                    current = process_whitespace(result, current, end);
                    continue;
                }

                result.push_back(*current);
                ++current;
            }

            trim_trailing_spaces(result);
            return result;
        }

        constexpr auto fnv1a_64(string_view text) noexcept -> uint64_t
        {
            constexpr uint64_t fnv_offset_basis = 14695981039346656037ull;
            constexpr uint64_t fnv_prime = 1099511628211ull;

            auto hash_value = fnv_offset_basis;
            for (const auto character : text)
            {
                hash_value = (hash_value ^ static_cast<uint64_t>(static_cast<unsigned char>(character))) * fnv_prime;
            }
            return hash_value;
        }

        template <typename T>
        struct normalized_name_holder
        {
            static_assert(!detail::is_unsupported_type_scope(detail::raw_type_name<T>()),
                          "normalized_type_name: local function scope, lambda scope, and anonymous namespace types are not supported");

            static constexpr auto storage = detail::normalize_type_string(detail::raw_type_name<T>());
            static constexpr string_view value = storage.view();
            static constexpr uint64_t hash = detail::fnv1a_64(storage.view());
        };
    } // namespace detail

    template <typename T>
    constexpr auto get_type_name() noexcept -> string_view
    {
        auto here = detail::raw_type_name<T>();

        constexpr auto function_scope_prefix = string_view{"()::"};
        if (tempest::starts_with(here, function_scope_prefix))
        {
            here = string_view{here.begin() + function_scope_prefix.size(), here.end()};
        }

        constexpr auto lambda_scope_prefix = string_view{"<lambda()>::"};
        if (tempest::starts_with(here, lambda_scope_prefix))
        {
            here = string_view{here.begin() + lambda_scope_prefix.size(), here.end()};
        }

        constexpr auto enum_prefix = string_view{"enum "};
        if (tempest::starts_with(here, enum_prefix))
        {
            here = string_view{here.begin() + enum_prefix.size(), here.end()};
        }

        constexpr auto struct_prefix = string_view{"struct "};
        if (tempest::starts_with(here, struct_prefix))
        {
            here = string_view{here.begin() + struct_prefix.size(), here.end()};
        }

        constexpr auto class_prefix = string_view{"class "};
        if (tempest::starts_with(here, class_prefix))
        {
            here = string_view{here.begin() + class_prefix.size(), here.end()};
        }

        return here;
    }

    template <typename T>
    inline constexpr bool is_normalized_type_name_valid_v =
        !detail::is_unsupported_type_scope(detail::raw_type_name<T>());

    template <typename T>
    consteval auto normalized_type_name() noexcept -> string_view
    {
        static_assert(!detail::is_unsupported_type_scope(detail::raw_type_name<T>()),
                      "normalized_type_name: local function scope, lambda scope, and anonymous namespace types are not supported");
        return detail::normalized_name_holder<T>::value;
    }

    template <typename T>
    inline constexpr uint64_t normalized_type_hash_v = detail::normalized_name_holder<T>::hash;

    template <size_t N>
    struct string_literal
    {
        static constexpr size_t size = N;

        constexpr string_literal(const char (&str)[N])
        {
            tempest::copy_n(str, N, value);
        }

        char value[N];
    };

    template <string_literal Name>
    struct named_type
    {
        static constexpr array<char, sizeof(Name.value)> value = to_array(Name.value);
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

    template <typename... Ts>
    struct type_list_size;

    template <typename... Ts>
    struct type_list_size<type_list<Ts...>>
    {
        static constexpr size_t value = sizeof...(Ts);
    };

    template <typename T>
    inline constexpr size_t type_list_size_v = type_list_size<T>::value;

    namespace detail
    {
        // Get index of a type in a type list
        template <size_t N, typename T1, typename... Ts>
        struct type_list_n_helper : type_list_n_helper<N - 1, Ts...>
        {
        };

        template <typename T1, typename... Ts>
        struct type_list_n_helper<0, T1, Ts...>
        {
            using type = T1;
        };
    } // namespace detail

    template <size_t N, typename... Ts>
    struct type_list_type_at;

    template <size_t N, typename... Ts>
    struct type_list_type_at<N, type_list<Ts...>>
    {
        using type = detail::type_list_n_helper<N, Ts...>::type;
    };

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

    template <size_t N, typename... Ts>
    struct take_type;

    template <size_t N, typename... Ts>
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

    template <size_t N, typename T, typename... Ts>
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
        using type = conditional_t<C::template compare<T1, T2>(), type_list<T1, T2>, type_list<T2, T1>>;
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
            using type = conditional_t<
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
            [[nodiscard]] static size_t next() noexcept
            {
                static size_t value{0};
                return value++;
            }
        };

        // FN1VA Hash - https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function
        template <typename>
        struct fnv1a_traits;

        template <>
        struct fnv1a_traits<uint32_t>
        {
            using type = uint32_t;
            static constexpr uint32_t offset = 2166136261;
            static constexpr uint32_t prime = 16777619;
        };

        template <>
        struct fnv1a_traits<uint64_t>
        {
            using type = uint64_t;
            static constexpr uint64_t offset = 14695981039346656037ull;
            static constexpr uint64_t prime = 1099511628211ull;
        };

        template <typename C>
        struct hash_string_base
        {
            using value_type = C;
            using size_type = size_t;
            using hash_type = size_t;

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

            [[nodiscard]] static constexpr auto hash(const C* str, size_t len) noexcept
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

            template <size_t N>
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

            template <size_t N>
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
        [[nodiscard]] constexpr size_t get_type_hash() noexcept
        {
            string_view type_name = get_type_name<T>();
            return hash_string::from(type_name.data(), type_name.size());
        }
    } // namespace detail

    template <typename T, typename = void>
    struct type_index final
    {
        using id_type = size_t;

        [[nodiscard]] static id_type value() noexcept
        {
            static const id_type id = detail::type_index::next();
            return id;
        }
    };

    template <typename T, typename = void>
    struct type_hash final
    {
        static constexpr size_t value() noexcept
        {
            constexpr auto v = detail::get_type_hash<T>();
            return v;
        }
    };

    template <typename T, typename = void>
    struct type_name final
    {
        static constexpr tempest::string_view value() noexcept
        {
            return get_type_name<T>();
        }
    };

    class TEMPEST_API type_info final
    {
      public:
        template <typename T>
        constexpr type_info(in_place_type_t<T>) noexcept
            : _id{type_index<remove_cv_t<remove_reference_t<T>>>::value()},
              _hash{type_hash<remove_cv_t<remove_reference_t<T>>>::value()},
              _name{type_name<remove_cv_t<remove_reference_t<T>>>::value()}
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
        size_t _id;
        size_t _hash;
        string_view _name;
    };

    template <typename T>
    [[nodiscard]] const type_info& type_id() noexcept
    {
        using base = remove_cv_t<remove_reference_t<T>>;
        if constexpr (is_same_v<base, T>)
        {
            static type_info instance{in_place_type_t<T>{}};
            return instance;
        }
        else
        {
            return type_id<base>();
        }
    }

    template <size_t N>
    struct select_t : select_t<N - 1>
    {
    };

    template <>
    struct select_t<0>
    {
    };

    template <size_t N>
    inline constexpr select_t<N> select;

    template <typename T, typename = void>
    struct size_of : integral_constant<size_t, 0u>
    {
    };

    template <typename T>
    struct size_of<T, void_t<decltype(sizeof(T))>> : integral_constant<size_t, sizeof(T)>
    {
    };

    template <typename T>
    inline constexpr size_t size_of_v = size_of<T>::value;
} // namespace tempest::core

namespace tempest
{
    using core::is_normalized_type_name_valid_v;
    using core::normalized_type_hash_v;
    using core::normalized_type_name;
} // namespace tempest

#endif // tempest_core_meta_hpp