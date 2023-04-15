#ifndef tempest_math_sequential_hpp
#define tempest_math_sequential_hpp

#include "intrinsic_base.hpp"

namespace tempest::math
{
    union alignas(8) intrinsic<float, 2, intrinsic_instruction_type::SEQUENTIAL>
    {
        float data[2];
    };

    intrinsic<float, 2, intrinsic_instruction_type::SEQUENTIAL> sum(
        intrinsic<float, 2, intrinsic_instruction_type::SEQUENTIAL> lhs,
        intrinsic<float, 2, intrinsic_instruction_type::SEQUENTIAL> rhs)
    {
        intrinsic<float, 2, intrinsic_instruction_type::SEQUENTIAL> result;
        result.data[0] = lhs.data[0] + rhs.data[0];
        result.data[1] = lhs.data[1] + rhs.data[1];
        return result;
    }
}

#endif // tempest_math_sequential_hpp
