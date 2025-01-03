.data
    LCPI0_0     QWORD 32, 48
    LCPI0_1     BYTE 0, 0, 0, 0, 0, 0, 0, 0, 16, 0, 0, 0, 0, 0, 0, 0

.code
public tempest_math_aligned_assign_mul_mat4_mat4

$lhs = 8
$rhs = 16

tempest_math_aligned_assign_mul_mat4_mat4 PROC
    push    rbp
    mov     rbp, rsp
    and     rsp, -32
    sub     rsp, 96
    vmovaps xmm0, xmmword ptr [rdx]
    vmovaps xmm1, xmmword ptr [rdx + 16]
    vmovaps xmm2, xmmword ptr [rdx + 32]
    vmovaps xmm3, xmmword ptr [rdx + 48]
    xor     eax, eax
    vxorps  xmm4, xmm4, xmm4
$LBB0_1:
    vbroadcastss    xmm5, dword ptr [rcx + rax]
    vmulps  xmm5, xmm0, xmm5
    vaddps  xmm5, xmm5, xmm4
    vmovaps xmmword ptr [rsp + rax], xmm5
    vbroadcastss    xmm6, dword ptr [rcx + rax + 4]
    vmulps  xmm6, xmm1, xmm6
    vaddps  xmm5, xmm5, xmm6
    vmovaps xmmword ptr [rsp + rax], xmm5
    vbroadcastss    xmm6, dword ptr [rcx + rax + 8]
    vmulps  xmm6, xmm2, xmm6
    vaddps  xmm5, xmm5, xmm6
    vmovaps xmmword ptr [rsp + rax], xmm5
    vbroadcastss    xmm6, dword ptr [rcx + rax + 12]
    vmulps  xmm6, xmm3, xmm6
    vaddps  xmm5, xmm5, xmm6
    vmovaps xmmword ptr [rsp + rax], xmm5
    add     rax, 16
    cmp     rax, 64
    jne     $LBB0_1
    vmovaps ymm0, ymmword ptr [rsp]
    vmovaps xmmword ptr [rdx], xmm0
    vmovaps xmm0, xmmword ptr [rsp + 16]
    vmovaps xmmword ptr [rdx + 16], xmm0
    vmovaps ymm0, ymmword ptr [rsp + 32]
    vmovaps xmmword ptr [rdx + 32], xmm0
    vmovaps xmm0, xmmword ptr [rsp + 48]
    vmovaps xmmword ptr [rdx + 48], xmm0
    mov     rsp, rbp
    pop     rbp
    vzeroupper
    ret

tempest_math_aligned_assign_mul_mat4_mat4 ENDP

END