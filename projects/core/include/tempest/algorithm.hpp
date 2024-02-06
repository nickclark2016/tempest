#ifndef tempest_core_algorithm_hpp
#define tempest_core_algorithm_hpp

#include <iterator>
#include <memory>
#include <type_traits>

namespace tempest::core
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
} // namespace tempest::core

#endif // tempest_core_algorithm_hpp