#ifndef tempest_core_algorithm_hpp
#define tempest_core_algorithm_hpp

#include <tempest/api.hpp>
#include <tempest/comparators.hpp>
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
        static constexpr void evaluate(Fn func)
        {
            func(StartIdx);
            loop_unroller<IdxType, StartIdx + StepSize, EndIdx, StepSize>::evaluate(func);
        }
    };

    template <typename IdxType, IdxType StartIdx, IdxType EndIdx, IdxType StepSize>
    struct loop_unroller<IdxType, StartIdx, EndIdx, StepSize, false>
    {
        template <typename Fn>
        static constexpr void evaluate([[maybe_unused]] Fn func)
        {
            // no op
            // loop does not continue, as loop function evaluated to false
        }
    };

    template <auto Start, auto End, auto StepSize, typename Fn>
    constexpr void unroll_loop(Fn func)
    {
        static_assert(is_convertible_v<decltype(End), decltype(Start)>, "End cannot be converted to type of Start.");
        static_assert(is_convertible_v<decltype(StepSize), decltype(Start)>,
                      "StepSize cannot be converted to type of Start.");
        loop_unroller<decltype(Start), Start, static_cast<decltype(Start)>(End),
                      static_cast<decltype(Start)>(StepSize)>::evaluate(func);
    }

    // NOLINTBEGIN(bugprone-return-const-ref-from-parameter)
    template <typename T>
    [[nodiscard]] constexpr auto clamp(const T& val, const T& low, const T& high) noexcept -> const T&
    {
        if (val < low)
        {
            return low;
        }
        if (high < val)
        {
            return high;
        }
        return val;
    }

    template <typename T, typename Compare>
    [[nodiscard]] constexpr auto clamp(const T& val, const T& low, const T& high, Compare comp) -> const T&
    {
        if (comp(val, low))
        {
            return low;
        }
        if (comp(high, val))
        {
            return high;
        }
        return val;
    }
    // NOLINTEND(bugprone-return-const-ref-from-parameter)

    [[nodiscard]] constexpr auto fast_mod(const integral auto value, const integral auto mod) noexcept -> integral auto
    {
        return value & (mod - 1);
    }

    template <integral T>
    [[nodiscard]] constexpr auto is_bit_set(T number, T bit_index) noexcept -> bool
    {
        return (number >> bit_index) & static_cast<T>(1);
    }

    template <integral T>
    [[nodiscard]] constexpr auto set_bit(T number, T bit_index) noexcept -> T
    {
        return number | (static_cast<T>(1) << bit_index);
    }

    template <integral T>
    [[nodiscard]] constexpr auto clear_bit(T number, T bit_index) noexcept -> T
    {
        return number & ~(static_cast<T>(1) << bit_index);
    }

    template <integral T>
    [[nodiscard]] constexpr auto toggle_bit(T number, T bit_index) noexcept -> T
    {
        return number ^ (static_cast<T>(1) << bit_index);
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

        TEMPEST_API
        auto compare_bytes(const void* lhs, const void* rhs, size_t count) -> int;
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

    // NOLINTBEGIN(bugprone-return-const-ref-from-parameter)
    template <typename T>
    constexpr auto min(const T& lhs, const T& rhs) -> const T&
    {
        return (lhs < rhs) ? lhs : rhs;
    }

    template <typename T, typename Compare>
    constexpr auto min(const T& lhs, const T& rhs, Compare comp) -> const T&
    {
        return comp(lhs, rhs) ? lhs : rhs;
    }

    template <typename T>
    constexpr auto max(const T& lhs, const T& rhs) -> const T&
    {
        return (lhs > rhs) ? lhs : rhs;
    }

    template <typename T, typename Compare>
    constexpr auto max(const T& lhs, const T& rhs, Compare comp) -> const T&
    {
        return comp(lhs, rhs) ? rhs : lhs;
    }
    // NOLINTEND(bugprone-return-const-ref-from-parameter)

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
            It current_it = first;
            auto step = count / 2;
            tempest::advance(current_it, step);

            if (*current_it < value)
            {
                first = ++current_it;
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
            It current_it = first;
            auto step = count / 2;
            tempest::advance(current_it, step);

            if (comp(*current_it, value))
            {
                first = ++current_it;
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
            It current_it = first;
            auto step = count / 2;
            tempest::advance(current_it, step);

            if (!(value < *current_it))
            {
                first = ++current_it;
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
            It current_it = first;
            auto step = count / 2;
            tempest::advance(current_it, step);

            if (!comp(value, *current_it))
            {
                first = ++current_it;
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
            auto cmp_result = comp(*first1, *first2);
            if (cmp_result != 0)
            {
                return cmp_result;
            }

            exhausted1 = (++first1 == last1);
            exhausted2 = (++first2 == last2);
        }

        if (!exhausted1)
        {
            return tempest::strong_ordering::greater;
        }
        if (!exhausted2)
        {
            return tempest::strong_ordering::less;
        }
        return tempest::strong_ordering::equal;
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

    // NOLINTBEGIN(bugprone-easily-swappable-parameters)
    template <forward_iterator It, typename T>
    constexpr void replace(It first, It last, const T& old_value, const T& new_value)
    {
        for (; first != last; ++first)
        {
            if (*first == old_value)
            {
                *first = new_value;
            }
        }
    }
    // NOLINTEND(bugprone-easily-swappable-parameters)

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
    constexpr auto all_of(InputIt begin, InputIt end, UnaryPred predicate) -> bool
    {
        return find_if_not(begin, end, predicate) == end;
    }

    template <input_iterator InputIt, typename UnaryPred>
    constexpr auto any_of(InputIt begin, InputIt end, UnaryPred predicate) -> bool
    {
        return find_if(begin, end, predicate) != end;
    }

    template <input_iterator InputIt, typename UnaryPred>
    constexpr auto none_of(InputIt begin, InputIt end, UnaryPred predicate) -> bool
    {
        return find_if(begin, end, predicate) == end;
    }

    namespace detail
    {
        template <random_access_iterator RandomIt, typename Compare>
        constexpr void insertion_sort(RandomIt first, RandomIt last, Compare comp)
        {
            if (first == last)
            {
                return;
            }

            for (auto iter = first + 1; iter != last; ++iter)
            {
                if (comp(*iter, *(iter - 1)))
                {
                    auto val = tempest::move(*iter);
                    auto current = iter;
                    do
                    {
                        *current = tempest::move(*(current - 1));
                        --current;
                    } while (current != first && comp(val, *(current - 1)));
                    *current = tempest::move(val);
                }
            }
        }

        template <random_access_iterator RandomIt, typename Compare>
        constexpr void sift_down(RandomIt first, iter_difference_t<RandomIt> root, iter_difference_t<RandomIt> len,
                                 Compare comp)
        {
            auto val = tempest::move(*(first + root));
            while (2 * root + 1 < len)
            {
                auto child = 2 * root + 1;
                if (child + 1 < len && comp(*(first + child), *(first + child + 1)))
                {
                    ++child;
                }
                if (comp(val, *(first + child)))
                {
                    *(first + root) = tempest::move(*(first + child));
                    root = child;
                }
                else
                {
                    break;
                }
            }
            *(first + root) = tempest::move(val);
        }

        template <random_access_iterator RandomIt, typename Compare>
        constexpr void heap_sort(RandomIt first, RandomIt last, Compare comp)
        {
            using diff_t = iter_difference_t<RandomIt>;
            const auto len = static_cast<diff_t>(last - first);
            if (len <= 1)
            {
                return;
            }

            for (diff_t parent_idx = (len - 2) / 2; parent_idx >= 0; --parent_idx)
            {
                sift_down(first, parent_idx, len, comp);
                if (parent_idx == 0)
                {
                    break;
                }
            }

            for (diff_t end_idx = len - 1; end_idx > 0; --end_idx)
            {
                tempest::swap(*first, *(first + end_idx));
                sift_down(first, diff_t{0}, end_idx, comp);
            }
        }

        template <random_access_iterator RandomIt, typename Compare>
        constexpr auto median_of_three(RandomIt first_it, RandomIt mid_it, RandomIt last_it, Compare comp) -> RandomIt
        {
            if (comp(*first_it, *mid_it))
            {
                if (comp(*mid_it, *last_it))
                {
                    return mid_it;
                }
                return comp(*first_it, *last_it) ? last_it : first_it;
            }
            if (comp(*first_it, *last_it))
            {
                return first_it;
            }
            return comp(*mid_it, *last_it) ? last_it : mid_it;
        }

        template <random_access_iterator RandomIt, typename Compare>
        constexpr void introsort_loop(RandomIt first, RandomIt last, iter_difference_t<RandomIt> depth_limit,
                                      Compare comp)
        {
            constexpr iter_difference_t<RandomIt> insertion_sort_threshold = 16;
            while (last - first > insertion_sort_threshold)
            {
                if (depth_limit == 0)
                {
                    heap_sort(first, last, comp);
                    return;
                }
                --depth_limit;

                auto mid = first + (last - first) / 2;
                auto pivot_it = median_of_three(first, mid, last - 1, comp);
                tempest::swap(*pivot_it, *(last - 1));

                const auto& pivot = *(last - 1);

                auto left_it = first;
                auto right_it = last - 2;

                while (true)
                {
                    while (left_it <= right_it && comp(*left_it, pivot))
                    {
                        ++left_it;
                    }
                    while (left_it <= right_it && comp(pivot, *right_it))
                    {
                        --right_it;
                    }
                    if (left_it >= right_it)
                    {
                        break;
                    }
                    tempest::swap(*left_it, *right_it);
                    ++left_it;
                    --right_it;
                }
                tempest::swap(*left_it, *(last - 1));

                if (left_it - first < last - (left_it + 1))
                {
                    introsort_loop(first, left_it, depth_limit, comp);
                    first = left_it + 1;
                }
                else
                {
                    introsort_loop(left_it + 1, last, depth_limit, comp);
                    last = left_it;
                }
            }
            insertion_sort(first, last, comp);
        }
    } // namespace detail

    /// @brief Sorts the elements in the range [first, last) in non-descending order using comp.
    /// @tparam RandomIt Random-access iterator type.
    /// @tparam Compare Comparison function object type.
    /// @param first Beginning of range to sort.
    /// @param last End of range to sort.
    /// @param comp Binary predicate taking two elements and returning true if first is less than second.
    template <random_access_iterator RandomIt, typename Compare>
    constexpr void sort(RandomIt first, RandomIt last, Compare comp)
    {
        const auto count = last - first;
        if (count <= 1)
        {
            return;
        }

        using diff_t = iter_difference_t<RandomIt>;
        auto depth_limit = diff_t{0};
        for (auto temp = count >> 1; temp > 0; temp >>= 1)
        {
            ++depth_limit;
        }
        depth_limit *= 2;

        detail::introsort_loop(first, last, depth_limit, comp);
    }

    /// @brief Sorts the elements in the range [first, last) in non-descending order using operator<.
    /// @tparam RandomIt Random-access iterator type.
    /// @param first Beginning of range to sort.
    /// @param last End of range to sort.
    template <random_access_iterator RandomIt>
    constexpr void sort(RandomIt first, RandomIt last)
    {
        tempest::sort(first, last, tempest::less<>{});
    }
} // namespace tempest

#endif // tempest_core_algorithm_hpp