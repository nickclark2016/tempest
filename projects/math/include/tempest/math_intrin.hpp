#ifndef tempest_math_math_intrin_hpp__
#define tempest_math_math_intrin_hpp__

extern "C" void tempest_math_aligned_mul_mat4_mat4(const float* lhs, const float* rhs, float* result);
extern "C" void tempest_math_aligned_assign_mul_mat4_mat4(float* lhs, const float* rhs);

#endif // tempest_math_math_intrin_hpp__