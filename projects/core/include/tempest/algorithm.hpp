#ifndef tempest_core_algorithm_hpp
#define tempest_core_algorithm_hpp

#include <tempest/concepts.hpp>
#include <tempest/iterator.hpp>
#include <tempest/type_traits.hpp>

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
        inline static constexpr void evaluate(Fn f)
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
    [[nodiscard]] inline constexpr T set_bit(T n, T k, bool x) noexcept
    {
        return (n & ~(static_cast<T>(1) << k)) | (static_cast<T>(1) << k);
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

        while (first != last)
        {
            *d_first++ = *first++;
        }
        return d_first;
    }

    template <input_iterator InputIt, typename Size, output_iterator<typename InputIt::value_type> OutputIt>
    inline constexpr OutputIt copy_n(InputIt first, Size count, OutputIt d_first)
    {
        using value_type = typename InputIt::value_type;

        if constexpr (is_trivial_v<value_type> && contiguous_iterator<InputIt>)
        {
            detail::copy_bytes(first, d_first, count * sizeof(value_type));
            return d_first + count;
        }

        for (Size i = 0; i < count; ++i)
        {
            *d_first++ = *first++;
        }
        return d_first;
    }
} // namespace tempest

#endif // tempest_core_algorithm_hpp