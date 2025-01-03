.code
public tempest_math_aligned_mul_mat4_mat4

$lhs = 8
$rhs = 16
$out = 24

tempest_math_aligned_mul_mat4_mat4 PROC
    vmovups xmm0, xmmword ptr [rdx]
    vmovups xmm1, xmmword ptr [rdx + 16]
    vmovups xmm2, xmmword ptr [rdx + 32]
    vmovups xmm3, xmmword ptr [rdx + 48]
    xor     eax, eax
    vxorps  xmm4, xmm4, xmm4
$LBB0_1:
    vbroadcastss    xmm5, dword ptr [rcx + rax]
    vmulps  xmm5, xmm0, xmm5
    vaddps  xmm5, xmm5, xmm4
    vmovups xmmword ptr [r8 + rax], xmm5
    vbroadcastss    xmm6, dword ptr [rcx + rax + 4]
    vmulps  xmm6, xmm1, xmm6
    vaddps  xmm5, xmm5, xmm6
    vmovups xmmword ptr [r8 + rax], xmm5
    vbroadcastss    xmm6, dword ptr [rcx + rax + 8]
    vmulps  xmm6, xmm2, xmm6
    vaddps  xmm5, xmm5, xmm6
    vmovups xmmword ptr [r8 + rax], xmm5
    vbroadcastss    xmm6, dword ptr [rcx + rax + 12]
    vmulps  xmm6, xmm3, xmm6
    vaddps  xmm5, xmm5, xmm6
    vmovups xmmword ptr [r8 + rax], xmm5
    add     rax, 16
    cmp     rax, 64
    jne     $LBB0_1
    vzeroupper
    ret

tempest_math_aligned_mul_mat4_mat4 ENDP

END
