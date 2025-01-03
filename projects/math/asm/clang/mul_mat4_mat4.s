# Signature:
# void tempest_math_aligned_mul_mat4_mat4(const float* lhs, const float* rhs, float* result);
#
# Requirements:
# - The result pointer must be aligned to 16 bytes.
# - The lhs and rhs pointers must be aligned to 16 bytes.
# - The data pointers are assumed to be valid.
# - The data pointers represent row major matrices

    .file "mul_mat4_mat4.s"
    .intel_syntax noprefix
    .text
.Ltext0:
    .p2align 4
    .globl tempest_math_aligned_mul_mat4_mat4
    .type  tempest_math_aligned_mul_mat4_mat4, @function
tempest_math_aligned_mul_mat4_mat4:
    vmovups xmm0, xmmword ptr [rsi]
    vmovups xmm1, xmmword ptr [rsi + 16]
    vmovups xmm2, xmmword ptr [rsi + 32]
    vmovups xmm3, xmmword ptr [rsi + 48]
    xor     eax, eax
    vxorps  xmm4, xmm4, xmm4
.LBB0_1:                                # =>This Inner Loop Header: Depth=1
    vbroadcastss    xmm5, dword ptr [rdi + rax]
    vmulps  xmm5, xmm0, xmm5
    vaddps  xmm5, xmm5, xmm4
    vmovups xmmword ptr [rdx + rax], xmm5
    vbroadcastss    xmm6, dword ptr [rdi + rax + 4]
    vmulps  xmm6, xmm1, xmm6
    vaddps  xmm5, xmm5, xmm6
    vmovups xmmword ptr [rdx + rax], xmm5
    vbroadcastss    xmm6, dword ptr [rdi + rax + 8]
    vmulps  xmm6, xmm2, xmm6
    vaddps  xmm5, xmm5, xmm6
    vmovups xmmword ptr [rdx + rax], xmm5
    vbroadcastss    xmm6, dword ptr [rdi + rax + 12]
    vmulps  xmm6, xmm3, xmm6
    vaddps  xmm5, xmm5, xmm6
    vmovups xmmword ptr [rdx + rax], xmm5
    add     rax, 16
    cmp     rax, 64
    jne     .LBB0_1
    vzeroupper
    ret
