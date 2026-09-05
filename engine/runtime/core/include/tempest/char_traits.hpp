#ifndef tempest_core_char_traits_hpp
#define tempest_core_char_traits_hpp

#include <tempest/algorithm.hpp>
#include <tempest/api.hpp>
#include <tempest/array.hpp>
#include <tempest/int.hpp>
#include <tempest/iterator.hpp>
#include <tempest/span.hpp>
#include <tempest/type_traits.hpp>
#include <tempest/vector.hpp>
#include <utility>

namespace tempest
{
    template <typename T>
    concept character_type = is_same_v<remove_cvref_t<T>, char> || is_same_v<remove_cvref_t<T>, wchar_t> ||
                             is_same_v<remove_cvref_t<T>, char8_t> || is_same_v<remove_cvref_t<T>, char16_t> ||
                             is_same_v<remove_cvref_t<T>, char32_t>;

    inline auto memmove(void* dst, const void* src, size_t count) -> void*
    {
        auto* dst_ptr = reinterpret_cast<byte*>(dst);
        const auto* src_ptr = reinterpret_cast<const byte*>(src);

        if (dst_ptr < src_ptr)
        {
            while ((count--) != 0U)
            {
                *dst_ptr++ = *src_ptr++;
            }
        }
        else
        {
            dst_ptr += (count - 1);
            src_ptr += (count - 1);

            while ((count--) != 0U)
            {
                *dst_ptr-- = *src_ptr--;
            }
        }

        return dst;
    }

    template <typename CharT>
    class char_traits;

    template <>
    class char_traits<char>
    {
      public:
        using char_type = char;
        using int_type = int;
        using off_type = long long;
        using pos_type = long long;

        static constexpr void assign(char_type& c1, const char_type& c2) noexcept;
        static constexpr auto assign(char_type* s, size_t n, char_type a) -> char_type*;

        static constexpr auto eq(char_type a, char_type b) noexcept -> bool;
        static constexpr auto lt(char_type a, char_type b) noexcept -> bool;

        static constexpr auto move(char_type* dest, const char_type* src, size_t count) -> char_type*;
        static constexpr auto copy(char_type* dest, const char_type* src, size_t count) -> char_type*;

        static constexpr auto compare(const char_type* s1, const char_type* s2, size_t count) -> int;
        static constexpr auto length(const char_type* s) -> size_t;

        static constexpr auto find(const char_type* ptr, size_t count, const char_type& ch) -> const char_type*;

        static constexpr auto to_char_type(int_type c) noexcept -> char_type;
        static constexpr auto to_int_type(char_type c) noexcept -> int_type;

        static constexpr auto eq_int_type(int_type c1, int_type c2) noexcept -> bool;

        static constexpr auto eof() noexcept -> int_type;

        static constexpr auto not_eof(int_type c) noexcept -> int_type;
    };

    template <>
    class char_traits<wchar_t>
    {
      public:
        using char_type = wchar_t;
        using int_type = wint_t;
        using off_type = long long;
        using pos_type = long long;

        static constexpr void assign(char_type& c1, const char_type& c2) noexcept;
        static constexpr auto assign(char_type* s, size_t n, char_type a) -> char_type*;

        static constexpr auto eq(char_type a, char_type b) noexcept -> bool;
        static constexpr auto lt(char_type a, char_type b) noexcept -> bool;

        static constexpr auto move(char_type* dest, const char_type* src, size_t count) -> char_type*;
        static constexpr auto copy(char_type* dest, const char_type* src, size_t count) -> char_type*;

        static constexpr auto compare(const char_type* s1, const char_type* s2, size_t count) -> int;
        static constexpr auto length(const char_type* s) -> size_t;

        static constexpr auto find(const char_type* ptr, size_t count, const char_type& ch) -> const char_type*;

        static constexpr auto to_char_type(int_type c) noexcept -> char_type;
        static constexpr auto to_int_type(char_type c) noexcept -> int_type;

        static constexpr auto eq_int_type(int_type c1, int_type c2) noexcept -> bool;

        static constexpr auto eof() noexcept -> int_type;

        static constexpr auto not_eof(int_type c) noexcept -> int_type;
    };

    constexpr void char_traits<char>::assign(char_type& c1, const char_type& c2) noexcept
    {
        c1 = c2;
    }

    constexpr auto char_traits<char>::assign(char_type* s, size_t n, char_type a) -> char_traits<char>::char_type*
    {
        for (size_t i = 0; i < n; ++i)
        {
            s[i] = a;
        }
        return s;
    }

    constexpr auto char_traits<char>::eq(char_type a, char_type b) noexcept -> bool
    {
        return a == b;
    }

    constexpr auto char_traits<char>::lt(char_type a, char_type b) noexcept -> bool
    {
        return a < b;
    }

    constexpr auto char_traits<char>::move(char_type* dest, const char_type* src,
                                                                           size_t count) -> char_traits<char>::char_type*
    {
        // For constant expressions, we need to fall back to a less efficient implementation to correctly handle
        // overlapping regions of memory.
        if (is_constant_evaluated())
        {
            bool should_loop_forward = true;

            for (const auto* s = src; s != dest; ++s)
            {
                if (s == dest)
                {
                    should_loop_forward = false;
                    break;
                }
            }

            if (should_loop_forward)
            {
                for (size_t i = 0; i < count; ++i)
                {
                    dest[i] = src[i];
                }
                return dest;
            }
            
            
                for (size_t i = count; i != 0; --i)
                {
                    dest[i - 1] = src[i - 1];
                }
                return dest;
           
        }

        (void)memmove(dest, src, count);
        return dest;
    }

    constexpr auto char_traits<char>::copy(char_type* dest, const char_type* src,
                                                                           size_t count) -> char_traits<char>::char_type*
    {
        // Assume: No overlapping regions
        for (size_t i = 0; i < count; ++i)
        {
            dest[i] = src[i];
        }

        return dest;
    }

    constexpr auto char_traits<char>::compare(const char_type* s1, const char_type* s2, size_t count) -> int
    {
        for (size_t i = 0; i < count; ++i)
        {
            auto diff = s1[i] - s2[i];
            if (diff != 0)
            {
                return diff;
            }
        }

        return 0;
    }

    constexpr auto char_traits<char>::length(const char_type* s) -> size_t
    {
        size_t len = 0;
        while (s[len] != char_type())
        {
            ++len;
        }
        return len;
    }

    constexpr auto char_traits<char>::find(const char_type* ptr, size_t count,
                                                                                 const char_type& ch) -> const char_traits<char>::char_type*
    {
        for (size_t i = 0; i < count; ++i)
        {
            if (ptr[i] == ch)
            {
                return ptr + i;
            }
        }

        return nullptr;
    }

    constexpr auto char_traits<char>::to_char_type(int_type c) noexcept -> char_traits<char>::char_type
    {
        return static_cast<char_type>(c);
    }

    constexpr auto char_traits<char>::to_int_type(char_type c) noexcept -> char_traits<char>::int_type
    {
        return static_cast<int_type>(c);
    }

    constexpr auto char_traits<char>::eq_int_type(int_type c1, int_type c2) noexcept -> bool
    {
        return c1 == c2;
    }

    constexpr auto char_traits<char>::eof() noexcept -> char_traits<char>::int_type
    {
        return -1; // EOF is -1
    }

    constexpr auto char_traits<char>::not_eof(int_type c) noexcept -> char_traits<char>::int_type
    {
        return c != eof() ? c : static_cast<int>(static_cast<int>(eof()) == 0);
    }

    constexpr void char_traits<wchar_t>::assign(char_type& c1, const char_type& c2) noexcept
    {
        c1 = c2;
    }

    constexpr auto char_traits<wchar_t>::assign(char_type* s, size_t n, char_type c) -> char_traits<wchar_t>::char_type*
    {
        for (size_t i = 0; i < n; ++i)
        {
            s[i] = c;
        }

        return s;
    }

    constexpr auto char_traits<wchar_t>::eq(char_type a, char_type b) noexcept -> bool
    {
        return a == b;
    }

    constexpr auto char_traits<wchar_t>::lt(char_type a, char_type b) noexcept -> bool
    {
        return a < b;
    }

    constexpr auto char_traits<wchar_t>::move(char_type* dest, const char_type* src,
                                                                                 size_t count) -> char_traits<wchar_t>::char_type*
    {
        // For constant expressions, we need to fall back to a less efficient implementation to correctly handle
        // overlapping regions of memory.
        if (is_constant_evaluated())
        {
            bool should_loop_forward = true;

            for (const auto* s = src; s != dest; ++s)
            {
                if (s == dest)
                {
                    should_loop_forward = false;
                    break;
                }
            }

            if (should_loop_forward)
            {
                for (size_t i = 0; i < count; ++i)
                {
                    dest[i] = src[i];
                }
                return dest;
            }
            
            
                for (size_t i = count; i != 0; --i)
                {
                    dest[i - 1] = src[i - 1];
                }
                return dest;
           
        }

        (void)memmove(dest, src, count * sizeof(wchar_t));
        return dest;
    }

    constexpr auto char_traits<wchar_t>::copy(char_type* dest, const char_type* src,
                                                                                 size_t count) -> char_traits<wchar_t>::char_type*
    {
        // Assume: No overlapping regions
        for (size_t i = 0; i < count; ++i)
        {
            dest[i] = src[i];
        }
        return dest;
    }

    constexpr auto char_traits<wchar_t>::compare(const char_type* s1, const char_type* s2, size_t count) -> int
    {
        for (size_t i = 0; i < count; ++i)
        {
            auto diff = s1[i] - s2[i];
            if (diff != 0)
            {
                return diff;
            }
        }
        return 0;
    }

    constexpr auto char_traits<wchar_t>::length(const char_type* s) -> size_t
    {
        size_t len = 0;
        while (s[len] != char_type())
        {
            ++len;
        }
        return len;
    }

    constexpr auto char_traits<wchar_t>::find(const char_type* ptr,
                                                                                       size_t count,
                                                                                       const char_type& ch) -> const char_traits<wchar_t>::char_type*
    {
        for (size_t i = 0; i < count; ++i)
        {
            if (ptr[i] == ch)
            {
                return ptr + i;
            }
        }
        return nullptr;
    }

    constexpr auto char_traits<wchar_t>::to_char_type(int_type c) noexcept -> char_traits<wchar_t>::char_type
    {
        return static_cast<char_type>(c);
    }

    constexpr auto char_traits<wchar_t>::to_int_type(char_type c) noexcept -> char_traits<wchar_t>::int_type
    {
        return static_cast<int_type>(c);
    }

    constexpr auto char_traits<wchar_t>::eq_int_type(int_type c1, int_type c2) noexcept -> bool
    {
        return c1 == c2;
    }

    constexpr auto char_traits<wchar_t>::eof() noexcept -> char_traits<wchar_t>::int_type
    {
        return static_cast<char_traits<wchar_t>::int_type>(-1); // EOF is -1
    }

    namespace detail
    {
        template <typename CharT, typename Traits>
        constexpr auto bad_character_heuristic(const CharT* str, size_t size,
                                                                    span<typename Traits::int_type> table) -> Traits::int_type
        {
            for (auto& entry : table)
            {
                entry = static_cast<Traits::int_type>(-1);
            }

            auto min_el = min_element(str, str + size);
            auto min_val = *min_el;

            for (size_t i = 0; i < size; ++i)
            {
                auto index = Traits::to_int_type(str[i]) - Traits::to_int_type(min_val);
                if (index >= 0 && index < static_cast<ptrdiff_t>(table.size()))
                {
                    table[Traits::to_int_type(str[i]) - Traits::to_int_type(min_val)] =
                        static_cast<Traits::int_type>(i);
                }
            }

            return min_val;
        }

        template <typename CharT, typename Traits>
        constexpr auto reverse_bad_character_heuristic(const CharT* str, size_t size,
                                                                            span<typename Traits::int_type> table) -> Traits::int_type
        {
            for (auto& entry : table)
            {
                entry = static_cast<Traits::int_type>(-1);
            }

            auto min_el = min_element(str, str + size);
            auto min_val = *min_el;

            for (size_t i = size; i > 0; --i)
            {
                auto it = i - 1;
                auto index = Traits::to_int_type(str[it]) - Traits::to_int_type(min_val);
                if (index >= 0 && static_cast<size_t>(index) < table.size())
                {
                    table[index] = static_cast<Traits::int_type>(it);
                }
            }

            return min_val;
        }

        template <typename CharT, typename Traits>
        constexpr auto boyer_moore_helper(const CharT* str, size_t str_len, const CharT* pattern,
                                                  size_t pattern_len, span<typename Traits::int_type> bad_char_table) -> const CharT*
        {
            auto min_value = bad_character_heuristic<CharT, Traits>(pattern, pattern_len, bad_char_table);

            for (ptrdiff_t s = 0; std::cmp_less_equal(s ,str_len - pattern_len);)
            {
                ptrdiff_t p = static_cast<ptrdiff_t>(pattern_len) - 1;

                while (p >= 0 && pattern[p] == str[s + p])
                {
                    --p;
                }

                if (p < 0)
                {
                    return str + s;
                }

                auto index = Traits::to_int_type(str[s + p]) - min_value;
                if (index < 0 || static_cast<size_t>(index) >= bad_char_table.size())
                {
                    s += 1;
                    continue;
                }

                s += max<typename Traits::int_type>(1, static_cast<Traits::int_type>(p) -
                                                           bad_char_table[Traits::to_int_type(str[s + p]) - min_value]);
            }

            return str + str_len;
        }

        template <typename CharT, typename Traits>
        constexpr auto reverse_boyer_more_helper(const CharT* str, size_t str_len, const CharT* pattern,
                                                         size_t pattern_len,
                                                         span<typename Traits::int_type> bad_char_table) -> const CharT*
        {
            auto s_pattern_len = static_cast<ptrdiff_t>(pattern_len);
            auto min_value = reverse_bad_character_heuristic<CharT, Traits>(pattern, pattern_len, bad_char_table);

            for (auto s = static_cast<ptrdiff_t>(str_len - pattern_len); s >= 0;)
            {
                ptrdiff_t p = 0;

                while (p < s_pattern_len && pattern[p] == str[s + p])
                {
                    ++p;
                }

                if (p == s_pattern_len)
                {
                    return str + s;
                }

                auto index = Traits::to_int_type(str[s + p]) - min_value;
                if (index < 0 || static_cast<size_t>(index) >= bad_char_table.size())
                {
                    s -= 1;
                    continue;
                }

                s -= max<typename Traits::int_type>(1, static_cast<Traits::int_type>(p) -
                                                           bad_char_table[Traits::to_int_type(str[s + p]) - min_value]);
            }

            return str + str_len;
        }

        template <typename CharT, typename Traits>
        constexpr auto boyer_moore(const CharT* str, size_t str_len, const CharT* pattern, size_t pattern_len) -> const CharT*
        {
            if constexpr (sizeof(CharT) == 1)
            {
                array<typename Traits::int_type, 256> bad_char_table;
                return boyer_moore_helper<CharT, Traits>(str, str_len, pattern, pattern_len, bad_char_table);
            }
            else
            {
                // Too large for stack, allocate on heap
                // Get the minimum and maximum values of the pattern
                auto [min_el, max_el] = minmax_element(pattern, pattern + pattern_len);

                vector<typename Traits::int_type> bad_char_table(*max_el - *min_el);
                return boyer_moore_helper<CharT, Traits>(str, str_len, pattern, pattern_len, bad_char_table);
            }
        }

        template <typename CharT, typename Traits>
        constexpr auto reverse_boyer_moore(const CharT* str, size_t str_len, const CharT* pattern,
                                                   size_t pattern_len) -> const CharT*
        {
            if constexpr (sizeof(CharT) == 1)
            {
                array<typename Traits::int_type, 256> bad_char_table;
                return reverse_boyer_more_helper<CharT, Traits>(str, str_len, pattern, pattern_len, bad_char_table);
            }
            else
            {
                // Too large for stack, allocate on heap
                // Get the minimum and maximum values of the pattern
                auto [min_el, max_el] = minmax_element(pattern, pattern + pattern_len);

                vector<typename Traits::int_type> bad_char_table(*max_el - *min_el);
                return reverse_boyer_more_helper<CharT, Traits>(str, str_len, pattern, pattern_len, bad_char_table);
            }
        }
    } // namespace detail

    template <typename It, character_type CharT>
    constexpr auto search(It first, It last, CharT ch) -> It
    {
        while (first != last && *first != ch)
        {
            ++first;
        }

        return first;
    }

    template <typename It, character_type CharT>
    constexpr auto search(It first, It last, const CharT* s, size_t count) -> It
    {
        using char_t = iterator_traits<It>::value_type;
        using traits_t = char_traits<char_t>;

        auto first_char_ptr = detail::boyer_moore<char_t, traits_t>(&(*first), last - first, s, count);
        const auto distance_to_first = first_char_ptr - &(*first);
        return next(first, distance_to_first);
    }

    template <typename It, typename It2>
        requires character_type<typename iterator_traits<It>::value_type> &&
                 character_type<typename iterator_traits<It2>::value_type>
    constexpr auto search(It first, It last, It2 p_first, It2 p_last) -> It
    {
        using char_t = iterator_traits<It>::value_type;
        using traits_t = char_traits<char_t>;

        auto first_char_ptr =
            detail::boyer_moore<char_t, traits_t>(&(*first), last - first, &(*p_first), p_last - p_first);
        const auto distance_to_first = first_char_ptr - &(*first);
        return next(first, distance_to_first);
    }

    template <typename It, typename It2>
        requires character_type<typename iterator_traits<It>::value_type> &&
                 character_type<typename iterator_traits<It2>::value_type>
    constexpr auto search_first_of(It first, It last, It2 p_first, It2 p_last) -> It
    {
        // Fast path for 8 bit characters
        if constexpr (sizeof(typename iterator_traits<It>::value_type) == 1)
        {
            // Create bit field for lookup
            uint64_t lut[4] = {0, 0, 0, 0}; // 256 bits, 0 initialized

            for (auto it = p_first; it != p_last; ++it)
            {
                auto ch = *it;
                auto idx = ch / 64;
                auto bit = ch % 64;
                lut[idx] |= 1ULL << bit;
            }

            for (; first != last; ++first)
            {
                auto ch = *first;
                auto idx = ch / 64;
                auto bit = ch % 64;
                if (lut[idx] & (1ULL << bit))
                {
                    return first;
                }
            }

            return last;
        }
        else
        {
            // Fallback for non 8 bit characters
            for (; first != last; ++first)
            {
                for (auto it = p_first; it != p_last; ++it)
                {
                    if (*first == *it)
                    {
                        return first;
                    }
                }
            }
            return last;
        }
    }

    template <typename It, character_type CharT>
        requires character_type<typename iterator_traits<It>::value_type>
    constexpr auto search_first_of(It first, It last, const CharT* s, size_t count) -> It
    {
        return search_first_of(first, last, s, s + count);
    }

    template <typename It, character_type CharT>
        requires character_type<typename iterator_traits<It>::value_type>
    constexpr auto search_first_of(It first, It last, const CharT* s) -> It
    {
        using traits = char_traits<CharT>;
        return search_first_of(first, last, s, traits::length(s));
    }

    template <typename It, character_type CharT>
        requires character_type<typename iterator_traits<It>::value_type>
    constexpr auto search_first_of(It first, It last, CharT ch) -> It
    {
        for (; first != last; ++first)
        {
            if (*first == ch)
            {
                return first;
            }
        }

        return last;
    }

    template <typename It, character_type CharT>
        requires character_type<typename iterator_traits<It>::value_type>
    constexpr auto reverse_search(It first, It last, CharT ch) -> It
    {
        for (auto it = last; it != first;)
        {
            --it;
            if (*it == ch)
            {
                return it;
            }
        }

        return last;
    }

    template <typename It, typename It2>
        requires character_type<typename iterator_traits<It>::value_type> &&
                 character_type<typename iterator_traits<It2>::value_type>
    constexpr auto reverse_search(It first, It last, It2 p_first, It2 p_last) -> It
    {
        using char_t = iterator_traits<It>::value_type;
        using traits_t = char_traits<char_t>;

        auto last_char_ptr =
            detail::reverse_boyer_moore<char_t, traits_t>(&(*first), last - first, &(*p_first), p_last - p_first);
        const auto distance_to_last = last_char_ptr - &(*first);
        return next(first, distance_to_last);
    }

    template <typename It, character_type CharT>
        requires character_type<typename iterator_traits<It>::value_type>
    constexpr auto reverse_search(It first, It last, const CharT* s, size_t count) -> It
    {
        using char_t = iterator_traits<It>::value_type;
        using traits_t = char_traits<char_t>;

        auto last_char_ptr = detail::reverse_boyer_moore<char_t, traits_t>(&(*first), last - first, s, count);
        const auto distance_to_last = last_char_ptr - &(*first);
        return next(first, distance_to_last);
    }

    template <typename It, character_type CharT>
        requires character_type<typename iterator_traits<It>::value_type>
    constexpr auto reverse_search(It first, It last, const CharT* s) -> It
    {
        using traits = char_traits<CharT>;
        return reverse_search(first, last, s, traits::length(s));
    }

    template <typename It, character_type CharT>
        requires character_type<typename iterator_traits<It>::value_type>
    constexpr auto search_last_of(It first, It last, CharT ch) -> It
    {
        return reverse_search(first, last, ch);
    }

    template <typename It, typename It2>
        requires character_type<typename iterator_traits<It>::value_type> &&
                 character_type<typename iterator_traits<It2>::value_type>
    constexpr auto search_last_of(It first, It last, It2 p_first, It2 p_last) -> It
    {
        // Fast path for 8 bit characters
        if constexpr (sizeof(typename iterator_traits<It>::value_type) == 1)
        {
            // Create bit field for lookup
            uint64_t lut[4] = {0, 0, 0, 0}; // 256 bits, 0 initialized

            for (auto it = p_first; it != p_last; ++it)
            {
                auto ch = *it;
                auto idx = ch / 64;
                auto bit = ch % 64;
                lut[idx] |= 1ULL << bit;
            }

            for (auto it = last; it != first;)
            {
                --it;
                auto ch = *it;
                auto idx = ch / 64;
                auto bit = ch % 64;
                if (lut[idx] & (1ULL << bit))
                {
                    return it;
                }
            }

            return last;
        }
        else
        {
            // Fallback for non 8 bit characters
            for (auto it = last; it != first;)
            {
                --it;
                for (auto it2 = p_first; it2 != p_last; ++it2)
                {
                    if (*it == *it2)
                    {
                        return it;
                    }
                }
            }

            return last;
        }
    }

    template <typename It, character_type CharT>
        requires character_type<typename iterator_traits<It>::value_type>
    constexpr auto search_last_of(It first, It last, const CharT* s, size_t count) -> It
    {
        return search_last_of(first, last, s, s + count);
    }

    template <typename It, character_type CharT>
        requires character_type<typename iterator_traits<It>::value_type>
    constexpr auto search_last_of(It first, It last, const CharT* s) -> It
    {
        using traits = char_traits<CharT>;
        return search_last_of(first, last, s, traits::length(s));
    }

    template <typename It, character_type CharT>
        requires character_type<typename iterator_traits<It>::value_type>
    constexpr auto search_first_not_of(It first, It last, CharT ch) -> It
    {
        for (; first != last; ++first)
        {
            if (*first != ch)
            {
                return first;
            }
        }

        return last;
    }

    template <typename It, typename It2>
        requires character_type<typename iterator_traits<It>::value_type> &&
                 character_type<typename iterator_traits<It2>::value_type>
    constexpr auto search_first_not_of(It first, It last, It2 p_first, It2 p_last) -> It
    {
        // Fast path for 8 bit characters
        if constexpr (sizeof(typename iterator_traits<It>::value_type) == 1)
        {
            // Create bit field for lookup
            uint64_t lut[4] = {0, 0, 0, 0}; // 256 bits, 0 initialized

            for (auto it = p_first; it != p_last; ++it)
            {
                auto ch = *it;
                auto idx = ch / 64;
                auto bit = ch % 64;
                lut[idx] |= 1ULL << bit;
            }

            for (; first != last; ++first)
            {
                auto ch = *first;
                auto idx = ch / 64;
                auto bit = ch % 64;
                if (!(lut[idx] & (1ULL << bit)))
                {
                    return first;
                }
            }

            return last;
        }
        else
        {
            // Fallback for non 8 bit characters
            for (; first != last; ++first)
            {
                bool found = false;
                for (auto it = p_first; it != p_last; ++it)
                {
                    if (*first == *it)
                    {
                        found = true;
                        break;
                    }
                }

                if (!found)
                {
                    return first;
                }
            }

            return last;
        }
    }

    template <typename It, character_type CharT>
        requires character_type<typename iterator_traits<It>::value_type>
    constexpr auto search_first_not_of(It first, It last, const CharT* s) -> It
    {
        using traits = char_traits<CharT>;
        return search_first_not_of(first, last, s, traits::length(s));
    }

    template <typename It, character_type CharT>
        requires character_type<typename iterator_traits<It>::value_type>
    constexpr auto search_first_not_of(It first, It last, const CharT* s, size_t count) -> It
    {
        return search_first_not_of(first, last, s, s + count);
    }

    template <typename It, character_type CharT>
        requires character_type<typename iterator_traits<It>::value_type>
    constexpr auto search_last_not_of(It first, It last, CharT ch) -> It
    {
        for (auto it = last; it != first;)
        {
            --it;
            if (*it != ch)
            {
                return it;
            }
        }

        return last;
    }

    template <typename It, typename It2>
        requires character_type<typename iterator_traits<It>::value_type> &&
                 character_type<typename iterator_traits<It2>::value_type>
    constexpr auto search_last_not_of(It first, It last, It2 p_first, It2 p_last) -> It
    {
        // Fast path for 8 bit characters
        if constexpr (sizeof(typename iterator_traits<It>::value_type) == 1)
        {
            // Create bit field for lookup
            uint64_t lut[4] = {0, 0, 0, 0}; // 256 bits, 0 initialized

            for (auto it = p_first; it != p_last; ++it)
            {
                auto ch = *it;
                auto idx = ch / 64;
                auto bit = ch % 64;
                lut[idx] |= 1ULL << bit;
            }

            for (auto it = last; it != first;)
            {
                --it;
                auto ch = *it;
                auto idx = ch / 64;
                auto bit = ch % 64;
                if (!(lut[idx] & (1ULL << bit)))
                {
                    return it;
                }
            }

            return last;
        }
        else
        {
            // Fallback for non 8 bit characters
            for (auto it = last; it != first;)
            {
                --it;
                bool found = false;
                for (auto it2 = p_first; it2 != p_last; ++it2)
                {
                    if (*it == *it2)
                    {
                        found = true;
                        break;
                    }
                }

                if (!found)
                {
                    return it;
                }
            }

            return last;
        }
    }

    template <typename It, character_type CharT>
        requires character_type<typename iterator_traits<It>::value_type>
    constexpr auto search_last_not_of(It first, It last, const CharT* s, size_t count) -> It
    {
        return search_last_not_of(first, last, s, s + count);
    }

    template <typename It, character_type CharT>
        requires character_type<typename iterator_traits<It>::value_type>
    constexpr auto search_last_not_of(It first, It last, const CharT* s) -> It
    {
        using traits = char_traits<CharT>;
        return search_last_not_of(first, last, s, traits::length(s));
    }

    template <typename It, character_type CharT>
        requires character_type<typename iterator_traits<It>::value_type>
    constexpr auto starts_with(It first, It last, CharT ch) -> bool
    {
        return first != last && *first == ch;
    }

    template <typename It, character_type CharT>
        requires character_type<typename iterator_traits<It>::value_type>
    constexpr auto starts_with(It first, It last, const CharT* s) -> bool
    {
        using traits = char_traits<CharT>;
        auto len = traits::length(s);

        return starts_with(first, last, s, len);
    }

    template <typename It, typename It2>
        requires character_type<typename iterator_traits<It>::value_type> &&
                 character_type<typename iterator_traits<It2>::value_type>
    constexpr auto starts_with(It first, It last, It2 p_first, It2 p_last) -> bool
    {
        while (first != last && p_first != p_last)
        {
            if (*first != *p_first)
            {
                return false;
            }

            ++first;
            ++p_first;
        }

        return p_first == p_last;
    }

    template <typename It, character_type CharT>
        requires character_type<typename iterator_traits<It>::value_type>
    constexpr auto starts_with(It first, It last, const CharT* s, size_t count) -> bool
    {
        return starts_with(first, last, s, s + count);
    }

    template <typename It, character_type CharT>
        requires character_type<typename iterator_traits<It>::value_type>
    constexpr auto ends_with(It first, It last, CharT ch) -> bool
    {
        if (first == last)
        {
            return false;
        }

        auto it = last;
        --it;
        return *it == ch;
    }

    template <typename It, character_type CharT>
        requires character_type<typename iterator_traits<It>::value_type>
    constexpr auto ends_with(It first, It last, const CharT* s) -> bool
    {
        using traits = char_traits<CharT>;
        auto len = traits::length(s);

        return ends_with(first, last, s, len);
    }

    template <typename It, typename It2>
        requires character_type<typename iterator_traits<It>::value_type> &&
                 character_type<typename iterator_traits<It2>::value_type>
    constexpr auto ends_with(It first, It last, It2 p_first, It2 p_last) -> bool
    {
        if (distance(first, last) < distance(p_first, p_last))
        {
            return false;
        }

        auto it = last;
        auto p_it = p_last;

        while (p_it != p_first)
        {
            --it;
            --p_it;

            if (*it != *p_it)
            {
                return false;
            }
        }

        return true;
    }

    template <typename It, character_type CharT>
        requires character_type<typename iterator_traits<It>::value_type>
    constexpr auto ends_with(It first, It last, const CharT* s, size_t count) -> bool
    {
        return ends_with(first, last, s, s + count);
    }

    template <typename It, character_type CharT>
        requires character_type<typename iterator_traits<It>::value_type>
    constexpr auto contains(It first, It last, CharT ch) -> bool
    {
        return search(first, last, ch) != last;
    }

    template <typename It, character_type CharT>
        requires character_type<typename iterator_traits<It>::value_type>
    constexpr auto contains(It first, It last, const CharT* s) -> bool
    {
        using traits = char_traits<CharT>;
        auto len = traits::length(s);

        return search(first, last, s, len) != last;
    }

    template <typename It, character_type CharT>
        requires character_type<typename iterator_traits<It>::value_type>
    constexpr auto contains(It first, It last, const CharT* s, size_t count) -> bool
    {
        return search(first, last, s, s + count) != last;
    }

    template <typename It, typename It2>
        requires character_type<typename iterator_traits<It>::value_type> &&
                 character_type<typename iterator_traits<It2>::value_type>
    constexpr auto contains(It first, It last, It2 p_first, It2 p_last) -> bool
    {
        return search(first, last, p_first, p_last) != last;
    }

    template <typename It, character_type CharT>
        requires character_type<typename iterator_traits<It>::value_type>
    constexpr auto compare(It first1, It last1, const CharT* s) -> int
    {
        using traits = char_traits<CharT>;
        auto len = traits::length(s);

        return compare(first1, last1, s, s + len);
    }

    template <typename It, character_type CharT>
        requires character_type<typename iterator_traits<It>::value_type>
    constexpr auto compare(It first1, It last1, const CharT* s, size_t count) -> int
    {
        return compare(first1, last1, s, s + count);
    }

    template <typename It, typename It2>
        requires character_type<typename iterator_traits<It>::value_type> &&
                 character_type<typename iterator_traits<It2>::value_type>
    constexpr auto compare(It first1, It last1, It2 first2, It2 last2) -> int
    {
        while (first1 != last1 && first2 != last2)
        {
            if (*first1 < *first2)
            {
                return -1;
            }

            if (*first1 > *first2)
            {
                return 1;
            }

            ++first1;
            ++first2;
        }

        if (first1 == last1 && first2 == last2)
        {
            return 0;
        }

        return first1 == last1 ? -1 : 1;
    }
} // namespace tempest

#endif // tempest_core_char_traits_hpp