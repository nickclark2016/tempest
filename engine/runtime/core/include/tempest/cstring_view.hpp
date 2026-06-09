#ifndef tempest_core_cstring_view_hpp
#define tempest_core_cstring_view_hpp

#include <tempest/api.hpp>
#include <tempest/assert.hpp>
#include <tempest/char_traits.hpp>
#include <tempest/concepts.hpp>
#include <tempest/hash.hpp>
#include <tempest/iterator.hpp>

namespace tempest
{
    template <typename T>
    concept cstring_like = requires(const T& val) {
        { val.c_str() } -> same_as<const typename T::value_type*>;
    };

    template <typename CharT, typename Traits = char_traits<CharT>>
    class TEMPEST_API basic_cstring_view
    {
      public:
        using value_type = CharT;
        using traits_type = Traits;
        using pointer = value_type*;
        using const_pointer = const value_type*;
        using reference = value_type&;
        using const_reference = const value_type&;
        using const_iterator = const_pointer;
        using iterator = const_iterator;
        using const_reverse_iterator = reverse_iterator<const_iterator>;
        using reverse_iterator = const_reverse_iterator;
        using size_type = size_t;
        using difference_type = ptrdiff_t;

        constexpr basic_cstring_view() noexcept = default;
        constexpr basic_cstring_view(const basic_cstring_view&) noexcept = default;
        constexpr basic_cstring_view(basic_cstring_view&&) noexcept = default;
        constexpr basic_cstring_view(const CharT* str) noexcept;
        constexpr basic_cstring_view(const CharT* str, size_type count) noexcept;

        template <contiguous_iterator It, sized_sentinel_for<It> End>
        constexpr basic_cstring_view(It first, End last) noexcept;

        template <typename R>
            requires(!same_as<remove_cvref_t<R>, basic_cstring_view<CharT, Traits>>)
        constexpr explicit basic_cstring_view(R&& rng) noexcept;

        constexpr basic_cstring_view(nullptr_t) = delete;

        constexpr ~basic_cstring_view() noexcept = default;

        constexpr basic_cstring_view& operator=(const basic_cstring_view&) noexcept = default;
        constexpr basic_cstring_view& operator=(basic_cstring_view&&) noexcept = default;

        [[nodiscard]] constexpr auto begin() const noexcept -> const_iterator;
        [[nodiscard]] constexpr auto cbegin() const noexcept -> const_iterator;
        [[nodiscard]] constexpr auto end() const noexcept -> const_iterator;
        [[nodiscard]] constexpr auto cend() const noexcept -> const_iterator;

        [[nodiscard]] constexpr auto rbegin() const noexcept -> const_reverse_iterator;
        [[nodiscard]] constexpr auto crbegin() const noexcept -> const_reverse_iterator;
        [[nodiscard]] constexpr auto rend() const noexcept -> const_reverse_iterator;
        [[nodiscard]] constexpr auto crend() const noexcept -> const_reverse_iterator;

        [[nodiscard]] constexpr auto size() const noexcept -> size_type;
        [[nodiscard]] constexpr auto length() const noexcept -> size_type;
        [[nodiscard]] constexpr auto max_size() const noexcept -> size_type;
        [[nodiscard]] constexpr auto empty() const noexcept -> bool;
        [[nodiscard]] constexpr auto data() const noexcept -> const_pointer;
        [[nodiscard]] constexpr auto c_str() const noexcept -> const_pointer;

        [[nodiscard]] constexpr auto operator[](size_type pos) const -> const_reference;
        [[nodiscard]] constexpr auto at(size_type pos) const -> const_reference;
        [[nodiscard]] constexpr auto front() const -> const_reference;
        [[nodiscard]] constexpr auto back() const -> const_reference;

      private:
        static constexpr CharT empty_string[1]{CharT(0)}; // NOLINT

        const CharT* _start = empty_string;
        const CharT* _end = empty_string;
    };

    template <typename CharT, typename Traits>
    constexpr auto operator==(basic_cstring_view<CharT, Traits> lhs, basic_cstring_view<CharT, Traits> rhs) noexcept
        -> bool
    {
        return lhs.size() == rhs.size() && Traits::compare(lhs.data(), rhs.data(), lhs.size()) == 0;
    }

    template <typename CharT, typename Traits>
    constexpr auto operator==(basic_cstring_view<CharT, Traits> lhs, const CharT* rhs) noexcept -> bool
    {
        return lhs.size() == Traits::length(rhs) && Traits::compare(lhs.data(), rhs, lhs.size()) == 0;
    }

    template <typename CharT, typename Traits>
    constexpr auto operator==(const CharT* lhs, basic_cstring_view<CharT, Traits> rhs) noexcept
    {
        return Traits::length(lhs) == rhs.size() && Traits::compare(lhs, rhs.data(), rhs.size()) == 0;
    }

    template <typename CharT, typename Traits>
    constexpr auto operator<=>(basic_cstring_view<CharT, Traits> lhs, basic_cstring_view<CharT, Traits> rhs) noexcept
    {
        auto lhs_size = lhs.size();
        auto rhs_size = rhs.size();

        auto result = Traits::compare(lhs.data(), rhs.data(), tempest::min(lhs_size, rhs_size));
        if (result != 0)
        {
            return result <=> 0;
        }

        return lhs_size <=> rhs_size;
    }

    template <typename CharT, typename Traits>
    constexpr basic_cstring_view<CharT, Traits>::basic_cstring_view(const CharT* str) noexcept
        : _start(str), _end(str + Traits::length(str))
    {
    }

    template <typename CharT, typename Traits>
    constexpr basic_cstring_view<CharT, Traits>::basic_cstring_view(const CharT* str, size_type count) noexcept
        : _start(str), _end(str + count)
    {
        TEMPEST_ASSERT((str[count] == CharT(0)) && "basic_cstring_view must be null-terminated");
    }

    template <typename CharT, typename Traits>
    template <contiguous_iterator It, sized_sentinel_for<It> End>
    constexpr basic_cstring_view<CharT, Traits>::basic_cstring_view(It first, End last) noexcept
        : _start(tempest::to_address(first)), _end(tempest::to_address(first) + tempest::distance(first, last))
    {
        TEMPEST_ASSERT(*_end == CharT(0) && "basic_cstring_view must be null-terminated");
    }

    template <typename CharT, typename Traits>
    template <typename R>
        requires(!same_as<remove_cvref_t<R>, basic_cstring_view<CharT, Traits>>)
    constexpr basic_cstring_view<CharT, Traits>::basic_cstring_view(R&& rng) noexcept
        : _start(tempest::data(forward<R>(rng))), _end(tempest::data(rng) + tempest::size(forward<R>(rng)))
    {
        TEMPEST_ASSERT(*_end == CharT(0) && "basic_cstring_view must be null-terminated");
    }

    template <typename CharT, typename Traits>
    constexpr auto basic_cstring_view<CharT, Traits>::begin() const noexcept -> const_iterator
    {
        return _start;
    }

    template <typename CharT, typename Traits>
    constexpr auto basic_cstring_view<CharT, Traits>::cbegin() const noexcept -> const_iterator
    {
        return _start;
    }

    template <typename CharT, typename Traits>
    constexpr auto basic_cstring_view<CharT, Traits>::end() const noexcept -> const_iterator
    {
        return _end;
    }

    template <typename CharT, typename Traits>
    constexpr auto basic_cstring_view<CharT, Traits>::cend() const noexcept -> const_iterator
    {
        return _end;
    }

    template <typename CharT, typename Traits>
    constexpr auto basic_cstring_view<CharT, Traits>::rbegin() const noexcept -> const_reverse_iterator
    {
        return make_reverse_iterator(end());
    }

    template <typename CharT, typename Traits>
    constexpr auto basic_cstring_view<CharT, Traits>::crbegin() const noexcept -> const_reverse_iterator
    {
        return make_reverse_iterator(end());
    }

    template <typename CharT, typename Traits>
    constexpr auto basic_cstring_view<CharT, Traits>::rend() const noexcept -> const_reverse_iterator
    {
        return make_reverse_iterator(begin());
    }

    template <typename CharT, typename Traits>
    constexpr auto basic_cstring_view<CharT, Traits>::crend() const noexcept -> const_reverse_iterator
    {
        return make_reverse_iterator(begin());
    }

    template <typename CharT, typename Traits>
    constexpr auto basic_cstring_view<CharT, Traits>::size() const noexcept -> size_type
    {
        return static_cast<size_type>(_end - _start);
    }

    template <typename CharT, typename Traits>
    constexpr auto basic_cstring_view<CharT, Traits>::length() const noexcept -> size_type
    {
        return size();
    }

    template <typename CharT, typename Traits>
    constexpr auto basic_cstring_view<CharT, Traits>::max_size() const noexcept -> size_type
    {
        return numeric_limits<size_type>::max();
    }

    template <typename CharT, typename Traits>
    constexpr auto basic_cstring_view<CharT, Traits>::empty() const noexcept -> bool
    {
        return _start == _end;
    }

    template <typename CharT, typename Traits>
    constexpr auto basic_cstring_view<CharT, Traits>::data() const noexcept -> const_pointer
    {
        return _start;
    }

    template <typename CharT, typename Traits>
    constexpr auto basic_cstring_view<CharT, Traits>::c_str() const noexcept -> const_pointer
    {
        return _start;
    }

    template <typename CharT, typename Traits>
    constexpr auto basic_cstring_view<CharT, Traits>::operator[](size_type pos) const -> const_reference
    {
        return _start[pos];
    }

    template <typename CharT, typename Traits>
    constexpr auto basic_cstring_view<CharT, Traits>::at(size_type pos) const -> const_reference
    {
        TEMPEST_ASSERT(pos < size());

        return _start[pos];
    }

    template <typename CharT, typename Traits>
    constexpr auto basic_cstring_view<CharT, Traits>::front() const -> const_reference
    {
        return *_start;
    }

    template <typename CharT, typename Traits>
    constexpr auto basic_cstring_view<CharT, Traits>::back() const -> const_reference
    {
        return *(_end - 1);
    }

    template <typename CharT, typename Traits>
    struct hash<basic_cstring_view<CharT, Traits>>
    {
        auto operator()(const basic_cstring_view<CharT, Traits>& view) const noexcept -> size_t
        {
            return detail::fnv1a_auto(view.data(), view.size());
        }
    };

    using cstring_view = basic_cstring_view<char>;
    using wcstring_view = basic_cstring_view<wchar_t>;

    namespace literals
    {
        [[nodiscard]] constexpr auto operator""_csv(const char* str, size_t len) noexcept -> cstring_view
        {
            return {str, len};
        }

        [[nodiscard]] constexpr auto operator""_wcsv(const wchar_t* str, size_t len) noexcept -> wcstring_view
        {
            return {str, len};
        }
    } // namespace literals

    template <typename CharT, typename Traits>
    [[nodiscard]] constexpr auto search(basic_cstring_view<CharT, Traits> view, CharT character) noexcept
    {
        return search(view.begin(), view.end(), character);
    }

    template <typename CharT, typename Traits>
    [[nodiscard]] constexpr auto search(basic_cstring_view<CharT, Traits> view, const CharT* str) noexcept
    {
        return search(view.begin(), view.end(), str, Traits::length(str));
    }

    template <typename CharT, typename Traits>
    [[nodiscard]] constexpr auto search(basic_cstring_view<CharT, Traits> view, const CharT* str, size_t count) noexcept
    {
        return search(view.begin(), view.end(), str, str + count);
    }

    template <typename CharT, typename Traits, typename It>
    [[nodiscard]] constexpr auto search(basic_cstring_view<CharT, Traits> view, It first, It last) noexcept
    {
        return search(view.begin(), view.end(), first, last);
    }

    template <typename CharT, typename Traits>
    [[nodiscard]] constexpr auto search(basic_cstring_view<CharT, Traits> view, basic_cstring_view<CharT, Traits> str) noexcept
    {
        return search(view.begin(), view.end(), str.begin(), str.end());
    }

    template <typename CharT, typename Traits>
    [[nodiscard]] constexpr auto reverse_search(basic_cstring_view<CharT, Traits> view, CharT character) noexcept
    {
        return reverse_search(view.begin(), view.end(), character);
    }

    template <typename CharT, typename Traits>
    [[nodiscard]] constexpr auto reverse_search(basic_cstring_view<CharT, Traits> view, const CharT* str) noexcept
    {
        return reverse_search(view.begin(), view.end(), str, Traits::length(str));
    }

    template <typename CharT, typename Traits>
    [[nodiscard]] constexpr auto reverse_search(basic_cstring_view<CharT, Traits> view, const CharT* str, size_t count) noexcept
    {
        return reverse_search(view.begin(), view.end(), str, str + count);
    }

    template <typename CharT, typename Traits, typename It>
    [[nodiscard]] constexpr auto reverse_search(basic_cstring_view<CharT, Traits> view, It first, It last) noexcept
    {
        return reverse_search(view.begin(), view.end(), first, last);
    }

    template <typename CharT, typename Traits>
    [[nodiscard]] constexpr auto reverse_search(basic_cstring_view<CharT, Traits> view, basic_cstring_view<CharT, Traits> str) noexcept
    {
        return reverse_search(view.begin(), view.end(), str.begin(), str.end());
    }

    template <typename CharT, typename Traits>
    [[nodiscard]] constexpr auto search_first_of(basic_cstring_view<CharT, Traits> view, CharT character) noexcept
    {
        return search_first_of(view.begin(), view.end(), character);
    }

    template <typename CharT, typename Traits>
    [[nodiscard]] constexpr auto search_first_of(basic_cstring_view<CharT, Traits> view, const CharT* str) noexcept
    {
        return search_first_of(view.begin(), view.end(), str, Traits::length(str));
    }

    template <typename CharT, typename Traits>
    [[nodiscard]] constexpr auto search_first_of(basic_cstring_view<CharT, Traits> view, const CharT* str, size_t count) noexcept
    {
        return search_first_of(view.begin(), view.end(), str, str + count);
    }

    template <typename CharT, typename Traits, typename It>
    [[nodiscard]] constexpr auto search_first_of(basic_cstring_view<CharT, Traits> view, It first, It last) noexcept
    {
        return search_first_of(view.begin(), view.end(), first, last);
    }

    template <typename CharT, typename Traits>
    [[nodiscard]] constexpr auto search_first_of(basic_cstring_view<CharT, Traits> view, basic_cstring_view<CharT, Traits> str) noexcept
    {
        return search_first_of(view.begin(), view.end(), str.begin(), str.end());
    }

    template <typename CharT, typename Traits>
    [[nodiscard]] constexpr auto search_first_not_of(basic_cstring_view<CharT, Traits> view, CharT character) noexcept
    {
        return search_first_not_of(view.begin(), view.end(), character);
    }

    template <typename CharT, typename Traits>
    [[nodiscard]] constexpr auto search_first_not_of(basic_cstring_view<CharT, Traits> view, const CharT* str) noexcept
    {
        return search_first_not_of(view.begin(), view.end(), str, Traits::length(str));
    }

    template <typename CharT, typename Traits>
    [[nodiscard]] constexpr auto search_first_not_of(basic_cstring_view<CharT, Traits> view, const CharT* str, size_t count) noexcept
    {
        return search_first_not_of(view.begin(), view.end(), str, str + count);
    }

    template <typename CharT, typename Traits, typename It>
    [[nodiscard]] constexpr auto search_first_not_of(basic_cstring_view<CharT, Traits> view, It first, It last) noexcept
    {
        return search_first_not_of(view.begin(), view.end(), first, last);
    }

    template <typename CharT, typename Traits>
    [[nodiscard]] constexpr auto search_first_not_of(basic_cstring_view<CharT, Traits> view, basic_cstring_view<CharT, Traits> str) noexcept
    {
        return search_first_not_of(view.begin(), view.end(), str.begin(), str.end());
    }

    template <typename CharT, typename Traits>
    [[nodiscard]] constexpr auto search_last_of(basic_cstring_view<CharT, Traits> view, CharT character) noexcept
    {
        return search_last_of(view.begin(), view.end(), character);
    }

    template <typename CharT, typename Traits>
    [[nodiscard]] constexpr auto search_last_of(basic_cstring_view<CharT, Traits> view, const CharT* str) noexcept
    {
        return search_last_of(view.begin(), view.end(), str, Traits::length(str));
    }

    template <typename CharT, typename Traits>
    [[nodiscard]] constexpr auto search_last_of(basic_cstring_view<CharT, Traits> view, const CharT* str, size_t count) noexcept
    {
        return search_last_of(view.begin(), view.end(), str, str + count);
    }

    template <typename CharT, typename Traits, typename It>
    [[nodiscard]] constexpr auto search_last_of(basic_cstring_view<CharT, Traits> view, It first, It last) noexcept
    {
        return search_last_of(view.begin(), view.end(), first, last);
    }

    template <typename CharT, typename Traits>
    [[nodiscard]] constexpr auto search_last_of(basic_cstring_view<CharT, Traits> view, basic_cstring_view<CharT, Traits> str) noexcept
    {
        return search_last_of(view.begin(), view.end(), str.begin(), str.end());
    }

    template <typename CharT, typename Traits>
    [[nodiscard]] constexpr auto search_last_not_of(basic_cstring_view<CharT, Traits> view, CharT character) noexcept
    {
        return search_last_not_of(view.begin(), view.end(), character);
    }

    template <typename CharT, typename Traits>
    [[nodiscard]] constexpr auto search_last_not_of(basic_cstring_view<CharT, Traits> view, const CharT* str) noexcept
    {
        return search_last_not_of(view.begin(), view.end(), str, Traits::length(str));
    }

    template <typename CharT, typename Traits>
    [[nodiscard]] constexpr auto search_last_not_of(basic_cstring_view<CharT, Traits> view, const CharT* str, size_t count) noexcept
    {
        return search_last_not_of(view.begin(), view.end(), str, str + count);
    }

    template <typename CharT, typename Traits, typename It>
    [[nodiscard]] constexpr auto search_last_not_of(basic_cstring_view<CharT, Traits> view, It first, It last) noexcept
    {
        return search_last_not_of(view.begin(), view.end(), first, last);
    }

    template <typename CharT, typename Traits>
    [[nodiscard]] constexpr auto search_last_not_of(basic_cstring_view<CharT, Traits> view, basic_cstring_view<CharT, Traits> str) noexcept
    {
        return search_last_not_of(view.begin(), view.end(), str.begin(), str.end());
    }

    template <typename CharT, typename Traits>
    [[nodiscard]] constexpr auto compare(basic_cstring_view<CharT, Traits> lhs, basic_cstring_view<CharT, Traits> rhs) noexcept 
    {
        return compare(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
    }

    template <typename CharT, typename Traits>
    [[nodiscard]] constexpr auto compare(basic_cstring_view<CharT, Traits> lhs, const CharT* rhs) noexcept
    {
        return compare(lhs.begin(), lhs.end(), rhs, rhs + Traits::length(rhs));
    }

    template <typename CharT, typename Traits>
    [[nodiscard]] constexpr auto compare(const CharT* lhs, basic_cstring_view<CharT, Traits> rhs) noexcept
    {
        return compare(lhs, lhs + Traits::length(lhs), rhs.begin(), rhs.end());
    }

    template <typename CharT, typename Traits>
    [[nodiscard]] constexpr auto starts_with(basic_cstring_view<CharT, Traits> lhs, basic_cstring_view<CharT, Traits> rhs) noexcept
    {
        return starts_with(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
    }

    template <typename CharT, typename Traits>
    [[nodiscard]] constexpr auto starts_with(basic_cstring_view<CharT, Traits> view, CharT character) noexcept
    {
        return starts_with(view.begin(), view.end(), character);
    }

    template <typename CharT, typename Traits>
    [[nodiscard]] constexpr auto starts_with(basic_cstring_view<CharT, Traits> view, const CharT* str) noexcept
    {
        return starts_with(view.begin(), view.end(), str, Traits::length(str));
    }

    template <typename CharT, typename Traits, typename It>
    [[nodiscard]] constexpr auto starts_with(basic_cstring_view<CharT, Traits> view, It first, It last) noexcept
    {
        return starts_with(view.begin(), view.end(), first, last);
    }

    template <typename CharT, typename Traits>
    [[nodiscard]] constexpr auto ends_with(basic_cstring_view<CharT, Traits> view, CharT character) noexcept
    {
        return ends_with(view.begin(), view.end(), character);
    }

    template <typename CharT, typename Traits>
    [[nodiscard]] constexpr auto ends_with(basic_cstring_view<CharT, Traits> view, const CharT* str) noexcept
    {
        return ends_with(view.begin(), view.end(), str, Traits::length(str));
    }

    template <typename CharT, typename Traits>
    [[nodiscard]] constexpr auto ends_with(basic_cstring_view<CharT, Traits> view, basic_cstring_view<CharT, Traits> str) noexcept
    {
        return ends_with(view.begin(), view.end(), str.begin(), str.end());
    }

    template <typename CharT, typename Traits, typename It>
    [[nodiscard]] constexpr auto ends_with(basic_cstring_view<CharT, Traits> view, It first, It last) noexcept
    {
        return ends_with(view.begin(), view.end(), first, last);
    }
} // namespace tempest

#endif // tempest_core_cstring_view_hpp
