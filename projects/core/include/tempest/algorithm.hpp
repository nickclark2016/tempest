#ifndef tempest_core_algorithm_hpp
#define tempest_core_algorithm_hpp

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
        inline static constexpr void evaluate(Fn f)
        {
            f(StartIdx);
            loop_unroller<IdxType, StartIdx + StepSize, EndIdx, StepSize>::evaluate(f);
        }
    };

    template <typename IdxType, IdxType StartIdx, IdxType EndIdx, IdxType StepSize>
    struct loop_unroller<IdxType, StartIdx, EndIdx, StepSize, false>
    {
        template <typename Fn>
        inline static constexpr void evaluate([[maybe_unused]] Fn f)
        {
            // no op
            // loop does not continue, as loop function evaluated to false
        }
    };

    template <auto Start, auto End, auto StepSize, typename Fn>
    inline constexpr void unroll_loop(Fn f)
    {
        static_assert(is_convertible_v<decltype(End), decltype(Start)>, "End cannot be converted to type of Start.");
        static_assert(is_convertible_v<decltype(StepSize), decltype(Start)>,
                      "StepSize cannot be converted to type of Start.");
        loop_unroller<decltype(Start), Start, static_cast<decltype(Start)>(End),
                      static_cast<decltype(Start)>(StepSize)>::evaluate(f);
    }

    [[nodiscard]] inline constexpr integral auto fast_mod(const integral auto value, const integral auto mod) noexcept
    {
        return value & (mod - 1);
    }

    template <integral T>
    [[nodiscard]] inline constexpr bool is_bit_set(T n, T k) noexcept
    {
        return (n >> k) & static_cast<T>(1);
    }

    template <integral T>
    [[nodiscard]] inline constexpr T set_bit(T n, T k) noexcept
    {
        return n | (static_cast<T>(1) << k);
    }

    template <integral T>
    [[nodiscard]] inline constexpr T clear_bit(T n, T k) noexcept
    {
        return n & ~(static_cast<T>(1) << k);
    }

    template <integral T>
    [[nodiscard]] inline constexpr T toggle_bit(T n, T k) noexcept
    {
        return n ^ (static_cast<T>(1) << k);
    }

    template <typename Iter, typename T>
    inline constexpr void fill(Iter begin, Iter end, const T& value)
    {
        for (auto it = begin; it != end; ++it)
        {
            *it = value;
        }
    }

    template <typename Iter, typename Count, typename T>
    inline constexpr void fill_n(Iter begin, Count count, const T& value)
    {
        for (Count i = 0; i < count; ++i)
        {
            *begin = value;
            ++begin;
        }
    }

    namespace detail
    {
        void copy_bytes(const void* src, void* dest, size_t count);
    }

    template <input_iterator InputIt, output_iterator<typename InputIt::value_type> OutputIt>
    inline constexpr OutputIt copy(InputIt first, InputIt last, OutputIt d_first)
    {
        using value_type = typename InputIt::value_type;

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
    inline constexpr OutputIt copy_n(InputIt first, Size count, OutputIt d_first)
    {
        if (count == 0)
        {
            return d_first;
        }

        using value_type = typename iterator_traits<InputIt>::value_type;

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

    template <input_iterator It, typename T = typename iterator_traits<It>::value_type>
    [[nodiscard]] inline constexpr It find(It first, It last, const T& value)
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

    template <forward_iterator It>
    [[nodiscard]] inline constexpr It min_element(It first, It last)
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
    [[nodiscard]] inline constexpr It min_element(It first, It last, Compare comp)
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
    [[nodiscard]] inline constexpr It max_element(It first, It last)
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
    [[nodiscard]] inline constexpr It max_element(It first, It last, Compare comp)
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
    inline constexpr const T& min(const T& a, const T& b)
    {
        return (a < b) ? a : b;
    }

    template <typename T, typename Compare>
    inline constexpr const T& min(const T& a, const T& b, Compare comp)
    {
        return comp(a, b) ? a : b;
    }

    template <typename T>
    inline constexpr const T& max(const T& a, const T& b)
    {
        return (a > b) ? a : b;
    }

    template <typename T, typename Compare>
    inline constexpr const T& max(const T& a, const T& b, Compare comp)
    {
        return comp(a, b) ? b : a;
    }

    template <forward_iterator It>
    [[nodiscard]] inline constexpr pair<It, It> minmax_element(It first, It last)
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
    [[nodiscard]] inline constexpr pair<It, It> minmax_element(It first, It last, Compare comp)
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

    template <forward_iterator It, typename T = typename iterator_traits<It>::value_type>
    inline constexpr It lower_bound(It first, It last, const T& value)
    {
        using diff_type = typename iterator_traits<It>::difference_type;
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

    template <forward_iterator It, typename T = typename iterator_traits<It>::value_type, typename Compare>
    inline constexpr It lower_bound(It first, It last, const T& value, Compare comp)
    {
        using diff_type = typename iterator_traits<It>::difference_type;
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

    template <forward_iterator It, typename T = typename iterator_traits<It>::value_type>
    inline constexpr It upper_bound(It first, It last, const T& value)
    {
        using diff_type = typename iterator_traits<It>::difference_type;
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

    template <forward_iterator It, typename T = typename iterator_traits<It>::value_type, typename Compare>
    inline constexpr It upper_bound(It first, It last, const T& value, Compare comp)
    {
        using diff_type = typename iterator_traits<It>::difference_type;
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
    inline constexpr bool equal(It1 first1, It1 last1, It2 first2)
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
} // namespace tempest

#endif // tempest_core_algorithm_hpp