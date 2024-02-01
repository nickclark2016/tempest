#ifndef tempest_core_algorithm_hpp
#define tempest_core_algorithm_hpp

#include <iterator>
#include <memory>
#include <type_traits>

namespace tempest::core
{
    template <typename InputIt, typename OutputIt>
    inline constexpr OutputIt copy_construct(InputIt first, InputIt last, OutputIt d_first)
    {
        using src_traits = std::iterator_traits<InputIt>;
        using dst_traits = std::iterator_traits<OutputIt>;

        for (; first != last; (void)++first, (void)++d_first)
        {
            typename dst_traits::pointer ptr = std::addressof(*d_first);
            std::construct_at(ptr, *first);
        }

        return d_first;
    }

    template <typename InputIt, typename OutputIt>
    inline constexpr OutputIt move_construct(InputIt first, InputIt last, OutputIt d_first)
    {
        using src_traits = std::iterator_traits<InputIt>;
        using dst_traits = std::iterator_traits<OutputIt>;

        for (; first != last; (void)++first, (void)++d_first)
        {
            typename dst_traits::pointer ptr = std::addressof(*d_first);
            std::construct_at(ptr, std::move(*first));
        }

        return d_first;
    }

    template <typename InputIt, typename OutputIt>
    inline constexpr OutputIt optimal_construct(InputIt first, InputIt last, OutputIt d_first)
    {
        if constexpr (std::is_move_constructible_v<typename std::iterator_traits<OutputIt>::value_type>)
        {
            return move_construct(first, last, d_first);
        }
        else
        {
            return copy_construct(first, last, d_first);
        }
    }

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
} // namespace tempest::core

#endif // tempest_core_algorithm_hpp