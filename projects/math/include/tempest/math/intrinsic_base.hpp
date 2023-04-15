#ifndef tempest_math_intrinsic_base_hpp
#define tempest_math_intrinsic_base_hpp

#include <cstddef>

namespace tempest::math
{
    enum class intrinsic_instruction_type
    {
        SSE_AVX,
        SEQUENTIAL
    };

    template <typename T, std::size_t Count, intrinsic_instruction_type Type> union intrinsic {
    };

    template <typename T, std::size_t Count, intrinsic_instruction_type Type>
    [[nodiscard]] intrinsic<T, Count, Type> add(intrinsic<T, Count, Type> lhs, intrinsic<T, Count, Type> rhs) noexcept;

    template <typename T, std::size_t Count, intrinsic_instruction_type Type>
    [[nodiscard]] intrinsic<T, Count, Type> sub(intrinsic<T, Count, Type> lhs, intrinsic<T, Count, Type> rhs) noexcept;

    template <typename T, std::size_t Count, intrinsic_instruction_type Type>
    [[nodiscard]] intrinsic<T, Count, Type> mul(intrinsic<T, Count, Type> lhs, intrinsic<T, Count, Type> rhs) noexcept;

    template <typename T, std::size_t Count, intrinsic_instruction_type Type>
    [[nodiscard]] intrinsic<T, Count, Type> div(intrinsic<T, Count, Type> lhs, intrinsic<T, Count, Type> rhs) noexcept;
} // namespace tempest::math

#endif // tempest_math_intrinsic_hpp