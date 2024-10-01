#ifndef tempest_core_string_view_hpp
#define tempest_core_string_view_hpp

#include <tempest/char_traits.hpp>
#include <tempest/hash.hpp>
#include <tempest/int.hpp>
#include <tempest/iterator.hpp>

#include <iterator>
#include <limits>

namespace tempest
{
    template <typename CharT, typename Traits = char_traits<CharT>>
    class basic_string_view
    {
      public:
        using traits_type = Traits;
        using value_type = CharT;
        using pointer = CharT*;
        using const_pointer = const CharT*;
        using reference = CharT&;
        using const_reference = const CharT&;
        using size_type = size_t;
        using difference_type = std::ptrdiff_t;
        using iterator = pointer;
        using const_iterator = const_pointer;
        using reverse_iterator = std::reverse_iterator<iterator>;
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;

        constexpr basic_string_view() noexcept = default;
        constexpr basic_string_view(const basic_string_view&) noexcept = default;
        constexpr basic_string_view(const CharT* s, size_type count) noexcept;
        constexpr basic_string_view(const CharT* s) noexcept;

        template <std::contiguous_iterator It, std::sized_sentinel_for<It> End>
        constexpr basic_string_view(It first, End last) noexcept;

        template <typename R>
        constexpr explicit basic_string_view(R&& r) noexcept;

        constexpr basic_string_view(std::nullptr_t) = delete;

        constexpr basic_string_view& operator=(const basic_string_view&) noexcept = default;

        [[nodiscard]] constexpr const_iterator begin() const noexcept;
        [[nodiscard]] constexpr const_iterator cbegin() const noexcept;
        [[nodiscard]] constexpr const_iterator end() const noexcept;
        [[nodiscard]] constexpr const_iterator cend() const noexcept;

        [[nodiscard]] constexpr const_reverse_iterator rbegin() const noexcept;
        [[nodiscard]] constexpr const_reverse_iterator crbegin() const noexcept;
        [[nodiscard]] constexpr const_reverse_iterator rend() const noexcept;
        [[nodiscard]] constexpr const_reverse_iterator crend() const noexcept;

        [[nodiscard]] constexpr const_reference operator[](size_type pos) const;
        [[nodiscard]] constexpr const_reference at(size_type pos) const;
        [[nodiscard]] constexpr const_reference front() const;
        [[nodiscard]] constexpr const_reference back() const;

        [[nodiscard]] constexpr const_pointer data() const noexcept;
        [[nodiscard]] constexpr size_type size() const noexcept;
        [[nodiscard]] constexpr size_type length() const noexcept;
        [[nodiscard]] constexpr size_type max_size() const noexcept;
        [[nodiscard]] constexpr bool empty() const noexcept;

      private:
        const CharT* _start{nullptr};
        const CharT* _end{nullptr};
    };

    template <typename CharT, typename Traits>
    constexpr bool operator==(const basic_string_view<CharT, Traits>& lhs,
                              const basic_string_view<CharT, Traits>& rhs) noexcept
    {
        return lhs.size() == rhs.size() && Traits::compare(lhs.data(), rhs.data(), lhs.size()) == 0;
    }

    template <typename CharT, typename Traits>
    constexpr bool operator==(const basic_string_view<CharT, Traits>& lhs, const CharT* rhs) noexcept
    {
        return lhs == basic_string_view<CharT, Traits>(rhs);
    }

    template <typename CharT, typename Traits>
    constexpr auto operator<=>(const basic_string_view<CharT, Traits>& lhs,
                               const basic_string_view<CharT, Traits>& rhs) noexcept
    {
        auto lhs_size = lhs.size();
        auto rhs_size = rhs.size();

        auto result = Traits::compare(lhs.data(), rhs.data(), std::min(lhs_size, rhs_size));
        if (result != 0)
        {
            return result <=> 0;
        }

        return lhs_size <=> rhs_size;
    }

    template <typename CharT, typename Traits>
    constexpr basic_string_view<CharT, Traits>::basic_string_view(const CharT* s, size_type count) noexcept
        : _start(s), _end(s + count)
    {
    }

    template <typename CharT, typename Traits>
    constexpr basic_string_view<CharT, Traits>::basic_string_view(const CharT* s) noexcept
        : _start(s), _end(s + Traits::length(s))
    {
    }

    template <typename CharT, typename Traits>
    template <std::contiguous_iterator It, std::sized_sentinel_for<It> End>
    constexpr basic_string_view<CharT, Traits>::basic_string_view(It first, End last) noexcept
        : _start(std::to_address(first)), _end(std::to_address(last))
    {
    }

    template <typename CharT, typename Traits>
    template <typename R>
    constexpr basic_string_view<CharT, Traits>::basic_string_view(R&& r) noexcept
        : _start(tempest::data(r)), _end(tempest::data(r) + tempest::size(r))
    {
    }

    template <typename CharT, typename Traits>
    inline constexpr auto basic_string_view<CharT, Traits>::begin() const noexcept -> const_iterator
    {
        return _start;
    }

    template <typename CharT, typename Traits>
    inline constexpr auto basic_string_view<CharT, Traits>::cbegin() const noexcept -> const_iterator
    {
        return _start;
    }

    template <typename CharT, typename Traits>
    inline constexpr auto basic_string_view<CharT, Traits>::end() const noexcept -> const_iterator
    {
        return _end;
    }

    template <typename CharT, typename Traits>
    inline constexpr auto basic_string_view<CharT, Traits>::cend() const noexcept -> const_iterator
    {
        return _end;
    }

    template <typename CharT, typename Traits>
    inline constexpr auto basic_string_view<CharT, Traits>::rbegin() const noexcept -> const_reverse_iterator
    {
        return const_reverse_iterator(_end);
    }

    template <typename CharT, typename Traits>
    inline constexpr auto basic_string_view<CharT, Traits>::crbegin() const noexcept -> const_reverse_iterator
    {
        return const_reverse_iterator(_end);
    }

    template <typename CharT, typename Traits>
    inline constexpr auto basic_string_view<CharT, Traits>::rend() const noexcept -> const_reverse_iterator
    {
        return const_reverse_iterator(_start);
    }

    template <typename CharT, typename Traits>
    inline constexpr auto basic_string_view<CharT, Traits>::crend() const noexcept -> const_reverse_iterator
    {
        return const_reverse_iterator(_start);
    }

    template <typename CharT, typename Traits>
    inline constexpr auto basic_string_view<CharT, Traits>::operator[](size_type pos) const -> const_reference
    {
        return _start[pos];
    }

    template <typename CharT, typename Traits>
    inline constexpr auto basic_string_view<CharT, Traits>::at(size_type pos) const -> const_reference
    {
        assert(pos < size());

        return _start[pos];
    }

    template <typename CharT, typename Traits>
    inline constexpr auto basic_string_view<CharT, Traits>::front() const -> const_reference
    {
        return *_start;
    }

    template <typename CharT, typename Traits>
    inline constexpr auto basic_string_view<CharT, Traits>::back() const -> const_reference
    {
        return *(_end - 1);
    }

    template <typename CharT, typename Traits>
    inline constexpr auto basic_string_view<CharT, Traits>::data() const noexcept -> const_pointer
    {
        return _start;
    }

    template <typename CharT, typename Traits>
    inline constexpr auto basic_string_view<CharT, Traits>::size() const noexcept -> size_type
    {
        return _end - _start;
    }

    template <typename CharT, typename Traits>
    inline constexpr auto basic_string_view<CharT, Traits>::length() const noexcept -> size_type
    {
        return size();
    }

    template <typename CharT, typename Traits>
    inline constexpr auto basic_string_view<CharT, Traits>::max_size() const noexcept -> size_type
    {
        return std::numeric_limits<size_type>::max();
    }

    template <typename CharT, typename Traits>
    inline constexpr auto basic_string_view<CharT, Traits>::empty() const noexcept -> bool
    {
        return _start == _end;
    }

    using string_view = basic_string_view<char>;

    namespace literals
    {
        [[nodiscard]] constexpr auto operator""_sv(const char* str, std::size_t len) noexcept -> string_view
        {
            return string_view(str, len);
        }
    } // namespace literals

    template <typename CharT, typename Traits>
    constexpr auto search(basic_string_view<CharT, Traits> sv, CharT ch) noexcept
    {
        return search(sv.begin(), sv.end(), ch);
    }

    template <typename CharT, typename Traits>
    constexpr auto search(basic_string_view<CharT, Traits> sv, const CharT* str) noexcept
    {
        return search(sv.begin(), sv.end(), str, Traits::length(str));
    }

    template <typename CharT, typename Traits>
    constexpr auto search(basic_string_view<CharT, Traits> sv, const CharT* str, std::size_t count) noexcept
    {
        return search(sv.begin(), sv.end(), str, str + count);
    }

    template <typename CharT, typename Traits>
    constexpr auto search(basic_string_view<CharT, Traits> sv, basic_string_view<CharT, Traits> str) noexcept
    {
        return search(sv.begin(), sv.end(), str.begin(), str.end());
    }

    template <typename CharT, typename Traits, typename It>
    constexpr auto search(basic_string_view<CharT, Traits> sv, It first, It last) noexcept
    {
        return search(sv.begin(), sv.end(), first, last);
    }

    template <typename CharT, typename Traits>
    constexpr auto reverse_search(basic_string_view<CharT, Traits> sv, CharT ch) noexcept
    {
        return reverse_search(sv.begin(), sv.end(), ch);
    }

    template <typename CharT, typename Traits>
    constexpr auto reverse_search(basic_string_view<CharT, Traits> sv, const CharT* str) noexcept
    {
        return reverse_search(sv.begin(), sv.end(), str, Traits::length(str));
    }

    template <typename CharT, typename Traits>
    constexpr auto reverse_search(basic_string_view<CharT, Traits> sv, const CharT* str, std::size_t count) noexcept
    {
        return reverse_search(sv.begin(), sv.end(), str, str + count);
    }

    template <typename CharT, typename Traits>
    constexpr auto reverse_search(basic_string_view<CharT, Traits> sv, basic_string_view<CharT, Traits> str) noexcept
    {
        return reverse_search(sv.begin(), sv.end(), str.begin(), str.end());
    }

    template <typename CharT, typename Traits, typename It>
    constexpr auto reverse_search(basic_string_view<CharT, Traits> sv, It first, It last) noexcept
    {
        return reverse_search(sv.begin(), sv.end(), first, last);
    }

    template <typename CharT, typename Traits>
    constexpr auto search_first_of(basic_string_view<CharT, Traits> sv, CharT ch) noexcept
    {
        return search_first_of(sv.begin(), sv.end(), ch);
    }

    template <typename CharT, typename Traits>
    constexpr auto search_first_of(basic_string_view<CharT, Traits> sv, const CharT* str) noexcept
    {
        return search_first_of(sv.begin(), sv.end(), str, Traits::length(str));
    }

    template <typename CharT, typename Traits>
    constexpr auto search_first_of(basic_string_view<CharT, Traits> sv, const CharT* str, std::size_t count) noexcept
    {
        return search_first_of(sv.begin(), sv.end(), str, str + count);
    }

    template <typename CharT, typename Traits>
    constexpr auto search_first_of(basic_string_view<CharT, Traits> sv, basic_string_view<CharT, Traits> str) noexcept
    {
        return search_first_of(sv.begin(), sv.end(), str.begin(), str.end());
    }

    template <typename CharT, typename Traits, typename It>
    constexpr auto search_first_of(basic_string_view<CharT, Traits> sv, It first, It last) noexcept
    {
        return search_first_of(sv.begin(), sv.end(), first, last);
    }

    template <typename CharT, typename Traits>
    constexpr auto search_first_not_of(basic_string_view<CharT, Traits> sv, CharT ch) noexcept
    {
        return search_first_not_of(sv.begin(), sv.end(), ch);
    }

    template <typename CharT, typename Traits>
    constexpr auto search_first_not_of(basic_string_view<CharT, Traits> sv, const CharT* str) noexcept
    {
        return search_first_not_of(sv.begin(), sv.end(), str, Traits::length(str));
    }

    template <typename CharT, typename Traits>
    constexpr auto search_first_not_of(basic_string_view<CharT, Traits> sv, const CharT* str,
                                       std::size_t count) noexcept
    {
        return search_first_not_of(sv.begin(), sv.end(), str, str + count);
    }

    template <typename CharT, typename Traits>
    constexpr auto search_first_not_of(basic_string_view<CharT, Traits> sv,
                                       basic_string_view<CharT, Traits> str) noexcept
    {
        return search_first_not_of(sv.begin(), sv.end(), str.begin(), str.end());
    }

    template <typename CharT, typename Traits, typename It>
    constexpr auto search_first_not_of(basic_string_view<CharT, Traits> sv, It first, It last) noexcept
    {
        return search_first_not_of(sv.begin(), sv.end(), first, last);
    }

    template <typename CharT, typename Traits>
    constexpr auto search_last_of(basic_string_view<CharT, Traits> sv, CharT ch) noexcept
    {
        return search_last_of(sv.begin(), sv.end(), ch);
    }

    template <typename CharT, typename Traits>
    constexpr auto search_last_of(basic_string_view<CharT, Traits> sv, const CharT* str) noexcept
    {
        return search_last_of(sv.begin(), sv.end(), str, Traits::length(str));
    }

    template <typename CharT, typename Traits>
    constexpr auto search_last_of(basic_string_view<CharT, Traits> sv, const CharT* str, std::size_t count) noexcept
    {
        return search_last_of(sv.begin(), sv.end(), str, str + count);
    }

    template <typename CharT, typename Traits>
    constexpr auto search_last_of(basic_string_view<CharT, Traits> sv, basic_string_view<CharT, Traits> str) noexcept
    {
        return search_last_of(sv.begin(), sv.end(), str.begin(), str.end());
    }

    template <typename CharT, typename Traits, typename It>
    constexpr auto search_last_of(basic_string_view<CharT, Traits> sv, It first, It last) noexcept
    {
        return search_last_of(sv.begin(), sv.end(), first, last);
    }

    template <typename CharT, typename Traits>
    constexpr auto search_last_not_of(basic_string_view<CharT, Traits> sv, CharT ch) noexcept
    {
        return search_last_not_of(sv.begin(), sv.end(), ch);
    }

    template <typename CharT, typename Traits>
    constexpr auto search_last_not_of(basic_string_view<CharT, Traits> sv, const CharT* str) noexcept
    {
        return search_last_not_of(sv.begin(), sv.end(), str, Traits::length(str));
    }

    template <typename CharT, typename Traits>
    constexpr auto search_last_not_of(basic_string_view<CharT, Traits> sv, const CharT* str, std::size_t count) noexcept
    {
        return search_last_not_of(sv.begin(), sv.end(), str, str + count);
    }

    template <typename CharT, typename Traits>
    constexpr auto search_last_not_of(basic_string_view<CharT, Traits> sv,
                                      basic_string_view<CharT, Traits> str) noexcept
    {
        return search_last_not_of(sv.begin(), sv.end(), str.begin(), str.end());
    }

    template <typename CharT, typename Traits, typename It>
    constexpr auto search_last_not_of(basic_string_view<CharT, Traits> sv, It first, It last) noexcept
    {
        return search_last_not_of(sv.begin(), sv.end(), first, last);
    }

    template <typename CharT, typename Traits>
    constexpr auto compare(basic_string_view<CharT, Traits> lhs, basic_string_view<CharT, Traits> rhs) noexcept
    {
        return compare(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
    }

    template <typename CharT, typename Traits>
    constexpr auto compare(basic_string_view<CharT, Traits> lhs, const CharT* rhs) noexcept
    {
        return compare(lhs.begin(), lhs.end(), rhs, rhs + Traits::length(rhs));
    }

    template <typename CharT, typename Traits>
    constexpr auto compare(const CharT* lhs, basic_string_view<CharT, Traits> rhs) noexcept
    {
        return compare(lhs, lhs + Traits::length(lhs), rhs.begin(), rhs.end());
    }

    template <typename CharT, typename Traits>
    constexpr auto compare(basic_string_view<CharT, Traits> lhs, const CharT* rhs, std::size_t count) noexcept
    {
        return compare(lhs.begin(), lhs.end(), rhs, rhs + count);
    }

    template <typename CharT, typename Traits>
    constexpr auto compare(const CharT* lhs, std::size_t count, basic_string_view<CharT, Traits> rhs) noexcept
    {
        return compare(lhs, lhs + count, rhs.begin(), rhs.end());
    }

    template <typename CharT, typename Traits>
    constexpr bool starts_with(basic_string_view<CharT, Traits> sv, CharT ch) noexcept
    {
        return starts_with(sv.begin(), sv.end(), ch);
    }

    template <typename CharT, typename Traits>
    constexpr bool starts_with(basic_string_view<CharT, Traits> sv, const CharT* str) noexcept
    {
        return starts_with(sv.begin(), sv.end(), str, Traits::length(str));
    }

    template <typename CharT, typename Traits>
    constexpr bool starts_with(basic_string_view<CharT, Traits> sv, basic_string_view<CharT, Traits> str) noexcept
    {
        return starts_with(sv.begin(), sv.end(), str.begin(), str.end());
    }

    template <typename CharT, typename Traits, typename It>
    constexpr bool starts_with(basic_string_view<CharT, Traits> sv, It first, It last) noexcept
    {
        return starts_with(sv.begin(), sv.end(), first, last);
    }

    template <typename CharT, typename Traits>
    constexpr bool ends_with(basic_string_view<CharT, Traits> sv, CharT ch) noexcept
    {
        return ends_with(sv.begin(), sv.end(), ch);
    }

    template <typename CharT, typename Traits>
    constexpr bool ends_with(basic_string_view<CharT, Traits> sv, const CharT* str) noexcept
    {
        return ends_with(sv.begin(), sv.end(), str, Traits::length(str));
    }

    template <typename CharT, typename Traits>
    constexpr bool ends_with(basic_string_view<CharT, Traits> sv, basic_string_view<CharT, Traits> str) noexcept
    {
        return ends_with(sv.begin(), sv.end(), str.begin(), str.end());
    }

    template <typename CharT, typename Traits, typename It>
    constexpr bool ends_with(basic_string_view<CharT, Traits> sv, It first, It last) noexcept
    {
        return ends_with(sv.begin(), sv.end(), first, last);
    }

    template <typename CharT, typename Traits>
    constexpr auto substr(basic_string_view<CharT, Traits> sv, std::size_t pos, std::size_t count) noexcept
    {
        return basic_string_view<CharT, Traits>(sv.begin() + pos, count);
    }

    template <typename CharT, typename Traits>
    struct hash<basic_string_view<CharT, Traits>>
    {
        size_t operator()(const basic_string_view<CharT, Traits>& sv) const noexcept
        {
            return detail::fnv1a_auto(sv.data(), sv.size());
        }
    };
} // namespace tempest

#endif // tempest_core_string_view_hpp
