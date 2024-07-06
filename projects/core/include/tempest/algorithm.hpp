#ifndef tempest_core_algorithm_hpp
#define tempest_core_algorithm_hpp

#include <iterator>
#include <memory>
#include <type_traits>

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
        static_assert(std::is_convertible_v<decltype(End), decltype(Start)>,
                      "End cannot be converted to type of Start.");
        static_assert(std::is_convertible_v<decltype(StepSize), decltype(Start)>,
                      "StepSize cannot be converted to type of Start.");
        loop_unroller<decltype(Start), Start, static_cast<decltype(Start)>(End),
                      static_cast<decltype(Start)>(StepSize)>::evaluate(f);
    }

    [[nodiscard]] inline constexpr std::integral auto fast_mod(const std::integral auto value,
                                                               const std::integral auto mod) noexcept
    {
        return value & (mod - 1);
    }

    template <std::integral T>
    [[nodiscard]] inline constexpr bool is_bit_set(T n, T k) noexcept
    {
        return (n >> k) & static_cast<T>(1);
    }

    template <std::integral T>
    [[nodiscard]] inline constexpr T set_bit(T n, T k) noexcept
    {
        return n | (static_cast<T>(1) << k);
    }

    template <std::integral T>
    [[nodiscard]] inline constexpr T set_bit(T n, T k, bool x) noexcept
    {
        return (n & ~(static_cast<T>(1) << k)) | (static_cast<T>(1) << k);
    }

    template <std::integral T>
    [[nodiscard]] inline constexpr T clear_bit(T n, T k) noexcept
    {
        return n & ~(static_cast<T>(1) << k);
    }

    template <std::integral T>
    [[nodiscard]] inline constexpr T toggle_bit(T n, T k) noexcept
    {
        return n ^ (static_cast<T>(1) << k);
    }

    template <typename T>
    [[nodiscard]] auto begin(T& container) -> decltype(container.begin())
    {
        return container.begin();
    }

    template <typename T>
    [[nodiscard]] auto begin(const T& container) -> decltype(container.begin())
    {
        return container.begin();
    }

    template <typename T>
    [[nodiscard]] auto end(T& container) -> decltype(container.end())
    {
        return container.end();
    }

    template <typename T>
    [[nodiscard]] auto end(const T& container) -> decltype(container.end())
    {
        return container.end();
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
} // namespace tempest

#endif // tempest_core_algorithm_hpp