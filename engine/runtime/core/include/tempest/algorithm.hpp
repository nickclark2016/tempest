#ifndef tempest_core_algorithm_hpp
#define tempest_core_algorithm_hpp

#include <tempest/api.hpp>
#include <tempest/compare.hpp>
#include <tempest/concepts.hpp>
#include <tempest/iterator.hpp>
#include <tempest/type_traits.hpp>
#include <tempest/utility.hpp>

namespace tempest
{
    template <typename IdxType, IdxType StartIdx, IdxType EndIdx, IdxType StepSize, bool Validate = (StartIdx < EndIdx)>
    struct loop_unroller
    {
        template <typename Fn>
        static constexpr void evaluate(Fn f)
        {
            f(StartIdx);
            loop_unroller<IdxType, StartIdx + StepSize, EndIdx, StepSize>::evaluate(f);
        }
    };

    template <typename IdxType, IdxType StartIdx, IdxType EndIdx, IdxType StepSize>
    struct loop_unroller<IdxType, StartIdx, EndIdx, StepSize, false>
    {
        template <typename Fn>
        static constexpr void evaluate([[maybe_unused]] Fn f)
        {
            // no op
            // loop does not continue, as loop function evaluated to false
        }
    };

    template <auto Start, auto End, auto StepSize, typename Fn>
    constexpr void unroll_loop(Fn f)
    {
        static_assert(is_convertible_v<decltype(End), decltype(Start)>, "End cannot be converted to type of Start.");
        static_assert(is_convertible_v<decltype(StepSize), decltype(Start)>,
                      "StepSize cannot be converted to type of Start.");
        loop_unroller<decltype(Start), Start, static_cast<decltype(Start)>(End),
                      static_cast<decltype(Start)>(StepSize)>::evaluate(f);
    }

    template <typename T>
    [[nodiscard]] constexpr auto clamp(const T& v, const T& lo, const T& hi) noexcept -> const T&
    {
        return (v < lo) ? lo : (hi < v) ? hi : v;
    }

    template <typename T, typename Compare>
    [[nodiscard]] constexpr auto clamp(const T& v, const T& lo, const T& hi, Compare comp) -> const T&
    {
        return comp(v, lo) ? lo : comp(hi, v) ? hi : v;
    }

    [[nodiscard]] constexpr auto fast_mod(const integral auto value, const integral auto mod) noexcept -> integral auto
    {
        return value & (mod - 1);
    }

    template <integral T>
    [[nodiscard]] constexpr auto is_bit_set(T n, T k) noexcept -> bool
    {
        return (n >> k) & static_cast<T>(1);
    }

    template <integral T>
    [[nodiscard]] constexpr auto set_bit(T n, T k) noexcept -> T
    {
        return n | (static_cast<T>(1) << k);
    }

    template <integral T>
    [[nodiscard]] constexpr auto clear_bit(T n, T k) noexcept -> T
    {
        return n & ~(static_cast<T>(1) << k);
    }

    template <integral T>
    [[nodiscard]] constexpr auto toggle_bit(T n, T k) noexcept -> T
    {
        return n ^ (static_cast<T>(1) << k);
    }

    template <typename Iter, typename T>
    constexpr void fill(Iter begin, Iter end, const T& value)
    {
        for (auto it = begin; it != end; ++it)
        {
            *it = value;
        }
    }

    template <typename Iter, typename Count, typename T>
    constexpr void fill_n(Iter begin, Count count, const T& value)
    {
        for (Count i = 0; i < count; ++i)
        {
            *begin = value;
            ++begin;
        }
    }

    namespace detail
    {
        TEMPEST_API
        void copy_bytes(const void* src, void* dest, size_t count);
    } // namespace detail

    template <input_iterator InputIt, output_iterator<typename InputIt::value_type> OutputIt>
    constexpr auto copy(InputIt first, InputIt last, OutputIt d_first) -> OutputIt
    {
        using value_type = InputIt::value_type;

        if constexpr (is_trivial_v<value_type> && contiguous_iterator<InputIt>)
        {
            const auto count = last - first;
            detail::copy_bytes(first, d_first, count * sizeof(value_type));
            return d_first + count;
        }
        else
        {
            while (first != last)
            {
                *d_first++ = *first++;
            }
            return d_first;
        }
    }

    template <input_iterator InputIt, typename Size,
              output_iterator<typename iterator_traits<InputIt>::value_type> OutputIt>
    constexpr auto copy_n(InputIt first, Size count, OutputIt d_first) -> OutputIt
    {
        if (count == 0)
        {
            return d_first;
        }

        using value_type = iterator_traits<InputIt>::value_type;

        if constexpr (is_trivial_v<value_type> && contiguous_iterator<InputIt>)
        {
            detail::copy_bytes(first, d_first, count * sizeof(value_type));
            return d_first + count;
        }
        else
        {
            for (Size i = 0; i < count; ++i)
            {
                *d_first++ = *first++;
            }
            return d_first;
        }
    }

    template <input_iterator It, typename T = iterator_traits<It>::value_type>
    [[nodiscard]] constexpr auto find(It first, It last, const T& value) -> It
    {
        while (first != last)
        {
            if (*first == value)
            {
                return first;
            }
            ++first;
        }
        return last;
    }

    template <input_iterator It, typename Compare>
    [[nodiscard]] constexpr auto find_if(It first, It last, Compare comp) -> It
    {
        while (first != last)
        {
            if (comp(*first))
            {
                return first;
            }
            ++first;
        }
        return last;
    }

    template <input_iterator It, typename Compare>
    [[nodiscard]] constexpr auto find_if_not(It first, It last, Compare comp) -> It
    {
        while (first != last)
        {
            if (!comp(*first))
            {
                return first;
            }
            ++first;
        }
        return last;
    }

    template <forward_iterator It>
    [[nodiscard]] constexpr auto min_element(It first, It last) -> It
    {
        if (first == last)
        {
            return last;
        }

        It min_it = first;
        ++first;

        while (first != last)
        {
            if (*first < *min_it)
            {
                min_it = first;
            }
            ++first;
        }

        return min_it;
    }

    template <forward_iterator It, typename Compare>
    [[nodiscard]] constexpr auto min_element(It first, It last, Compare comp) -> It
    {
        if (first == last)
        {
            return last;
        }

        It min_it = first;
        ++first;

        while (first != last)
        {
            if (comp(*first, *min_it))
            {
                min_it = first;
            }
            ++first;
        }
        return min_it;
    }

    template <forward_iterator It>
    [[nodiscard]] constexpr auto max_element(It first, It last) -> It
    {
        if (first == last)
        {
            return last;
        }

        It max_it = first;
        ++first;

        while (first != last)
        {
            if (*first > *max_it)
            {
                max_it = first;
            }
            ++first;
        }
        return max_it;
    }

    template <forward_iterator It, typename Compare>
    [[nodiscard]] constexpr auto max_element(It first, It last, Compare comp) -> It
    {
        if (first == last)
        {
            return last;
        }

        It max_it = first;
        ++first;

        while (first != last)
        {
            if (comp(*max_it, *first))
            {
                max_it = first;
            }
            ++first;
        }
        return max_it;
    }

    template <typename T>
    constexpr auto min(const T& a, const T& b) -> const T&
    {
        return (a < b) ? a : b;
    }

    template <typename T, typename Compare>
    constexpr auto min(const T& a, const T& b, Compare comp) -> const T&
    {
        return comp(a, b) ? a : b;
    }

    template <typename T>
    constexpr auto max(const T& a, const T& b) -> const T&
    {
        return (a > b) ? a : b;
    }

    template <typename T, typename Compare>
    constexpr auto max(const T& a, const T& b, Compare comp) -> const T&
    {
        return comp(a, b) ? b : a;
    }

    template <forward_iterator It>
    [[nodiscard]] constexpr auto minmax_element(It first, It last) -> pair<It, It>
    {
        if (first == last)
        {
            return {last, last};
        }

        It min_it = first;
        It max_it = first;
        ++first;

        while (first != last)
        {
            if (*first < *min_it)
            {
                min_it = first;
            }
            else if (*first > *max_it)
            {
                max_it = first;
            }
            ++first;
        }

        return {min_it, max_it};
    }

    template <forward_iterator It, typename Compare>
    [[nodiscard]] constexpr auto minmax_element(It first, It last, Compare comp) -> pair<It, It>
    {
        if (first == last)
        {
            return {last, last};
        }

        It min_it = first;
        It max_it = first;
        +first;

        while (first != last)
        {
            if (comp(*first, *min_it))
            {
                min_it = first;
            }
            else if (comp(*max_it, *first))
            {
                max_it = first;
            }
            ++first;
        }

        return {min_it, max_it};
    }

    template <forward_iterator It, typename T = iterator_traits<It>::value_type>
    constexpr auto lower_bound(It first, It last, const T& value) -> It
    {
        using diff_type = iterator_traits<It>::difference_type;
        diff_type count = distance(first, last);

        while (count > 0)
        {
            It it = first;
            auto step = count / 2;
            tempest::advance(it, step);

            if (*it < value)
            {
                first = ++it;
                count -= step + 1;
            }
            else
            {
                count = step;
            }
        }

        return first;
    }

    template <forward_iterator It, typename T = iterator_traits<It>::value_type, typename Compare>
    constexpr auto lower_bound(It first, It last, const T& value, Compare comp) -> It
    {
        using diff_type = iterator_traits<It>::difference_type;
        diff_type count = distance(first, last);

        while (count > 0)
        {
            It it = first;
            auto step = count / 2;
            tempest::advance(it, step);

            if (comp(*it, value))
            {
                first = ++it;
                count -= step + 1;
            }
            else
            {
                count = step;
            }
        }

        return first;
    }

    template <forward_iterator It, typename T = iterator_traits<It>::value_type>
    constexpr auto upper_bound(It first, It last, const T& value) -> It
    {
        using diff_type = iterator_traits<It>::difference_type;
        diff_type count = distance(first, last);

        while (count > 0)
        {
            It it = first;
            auto step = count / 2;
            tempest::advance(it, step);

            if (!(value < *it))
            {
                first = ++it;
                count -= step + 1;
            }
            else
            {
                count = step;
            }
        }

        return first;
    }

    template <forward_iterator It, typename T = iterator_traits<It>::value_type, typename Compare>
    constexpr auto upper_bound(It first, It last, const T& value, Compare comp) -> It
    {
        using diff_type = iterator_traits<It>::difference_type;
        diff_type count = distance(first, last);

        while (count > 0)
        {
            It it = first;
            auto step = count / 2;
            tempest::advance(it, step);

            if (!comp(value, *it))
            {
                first = ++it;
                count -= step + 1;
            }
            else
            {
                count = step;
            }
        }
        return first;
    }

    template <input_iterator It1, input_iterator It2>
    constexpr auto equal(It1 first1, It1 last1, It2 first2) -> bool
    {
        for (; first1 != last1; ++first1, ++first2)
        {
            if constexpr (requires {
                              { *first1 != *first2 } -> convertible_to<bool>;
                          })
            {
                if (*first1 != *first2)
                {
                    return false;
                }
            }
            else
            {
                if (!(*first1 == *first2))
                {
                    return false;
                }
            }
        }

        return true;
    }

    template <input_iterator It1, input_iterator It2>
    constexpr auto equal(It1 first1, It1 last1, It2 first2, It2 last2) -> bool
    {
        return (last1 - first1) == (last2 - first2) && equal(first1, last1, first2);
    }

    // TODO: Replace iterator with input_iterator when I fix flat_map's const iterator to be input_iterator compatible
    template <iterator It1, iterator It2, typename Compare>
    constexpr auto lexicographical_compare_three_way(It1 first1, It1 last1, It2 first2, It2 last2, Compare comp)
        -> decltype(comp(*first1, *first2))
    {
        bool exhausted1 = (first1 == last1);
        bool exhausted2 = (first2 == last2);

        while (!exhausted1 && !exhausted2)
        {
            auto c = comp(*first1, *first2);
            if (c != 0)
            {
                return c;
            }

            exhausted1 = (++first1 == last1);
            exhausted2 = (++first2 == last2);
        }

        return !exhausted1   ? tempest::strong_ordering::greater
               : !exhausted2 ? tempest::strong_ordering::less
                             : tempest::strong_ordering::equal;
    }

    template <iterator It1, iterator It2>
    constexpr auto lexicographical_compare_three_way(It1 first1, It1 last1, It2 first2, It2 last2)
    {
        return lexicographical_compare_three_way(first1, last1, first2, last2, tempest::compare_three_way{});
    }

    template <forward_iterator It, typename T = iterator_traits<It>::value_type>
    constexpr auto remove(It first, It last, const T& value) -> It
    {
        first = find(first, last, value);

        if (first != last)
        {
            for (auto it = first; ++it != last;)
            {
                // TODO - Optimize for case where != exists
                if (!(*it == value))
                {
                    *first++ = tempest::move(*it);
                }
            }
        }

        return first;
    }

    template <forward_iterator It, typename Compare>
    constexpr auto remove_if(It first, It last, Compare comp) -> It
    {
        first = find_if(first, last, comp);

        if (first != last)
        {
            for (auto it = first; ++it != last;)
            {
                if (!comp(*it))
                {
                    *first++ = tempest::move(*it);
                }
            }
        }

        return first;
    }

    template <forward_iterator It, typename T>
    constexpr void replace(It first, It last, const T& old, const T& new_value)
    {
        for (; first != last; ++first)
        {
            if (*first == old)
            {
                *first = new_value;
            }
        }
    }

    template <forward_iterator It, typename Compare, typename T>
    constexpr void replace_if(It first, It last, Compare comp, const T& new_value)
    {
        for (; first != last; ++first)
        {
            if (comp(*first))
            {
                *first = new_value;
            }
        }
    }

    template <input_iterator InputIt, typename UnaryPred>
    constexpr auto all_of(InputIt begin, InputIt end, UnaryPred p) -> bool
    {
        return find_if_not(begin, end, p) == end;
    }

    template <input_iterator InputIt, typename UnaryPred>
    constexpr auto any_of(InputIt begin, InputIt end, UnaryPred p) -> bool
    {
        return find_if(begin, end, p) != end;
    }

    template <input_iterator InputIt, typename UnaryPred>
    constexpr auto none_of(InputIt begin, InputIt end, UnaryPred p) -> bool
    {
        return find_if(begin, end, p) == end;
    }
} // namespace tempest

#endif // tempest_core_algorithm_hpp