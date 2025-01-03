.code
public tempest_math_aligned_assign_mul_mat4_mat4

$lhs = 8
$rhs = 16

tempest_math_aligned_assign_mul_mat4_mat4 PROC
        push    rbp
        mov     rbp, rsp
        and     rsp, -32
        sub     rsp, 96

        mov rdi, rcx
        mov rsi, rdx

        vmovss  xmm0, dword ptr [rsi]           
        vmovss  xmm1, dword ptr [rsi + 4]       
        vmovss  xmm2, dword ptr [rsi + 8]       
        vmovss  xmm3, dword ptr [rsi + 12]
        vinsertps       xmm0, xmm0, dword ptr [rsi + 16], 16 
        vinsertps       xmm0, xmm0, dword ptr [rsi + 32], 32 
        vinsertps       xmm0, xmm0, dword ptr [rsi + 48], 48 
        vinsertps       xmm1, xmm1, dword ptr [rsi + 20], 16 
        vinsertps       xmm1, xmm1, dword ptr [rsi + 36], 32 
        vinsertps       xmm1, xmm1, dword ptr [rsi + 52], 48 
        vinsertps       xmm2, xmm2, dword ptr [rsi + 24], 16 
        vinsertps       xmm2, xmm2, dword ptr [rsi + 40], 32 
        vinsertps       xmm2, xmm2, dword ptr [rsi + 56], 48 
        vinsertps       xmm3, xmm3, dword ptr [rsi + 28], 16 
        vinsertps       xmm3, xmm3, dword ptr [rsi + 44], 32 
        vinsertps       xmm3, xmm3, dword ptr [rsi + 60], 48 
        xor     eax, eax
        vxorps  xmm4, xmm4, xmm4
$LL0:
        vmovaps xmmword ptr [rsp + 4*rax], xmm4
        vbroadcastss    xmm5, dword ptr [rdi + rax + 16]
        vmulps  xmm5, xmm5, xmm1
        vbroadcastss    xmm6, dword ptr [rdi + rax]
        vmulps  xmm6, xmm6, xmm0
        vaddps  xmm6, xmm6, xmm4
        vaddps  xmm5, xmm6, xmm5
        vbroadcastss    xmm6, dword ptr [rdi + rax + 32]
        vmulps  xmm6, xmm6, xmm2
        vaddps  xmm5, xmm5, xmm6
        vbroadcastss    xmm6, dword ptr [rdi + rax + 48]
        vmulps  xmm6, xmm6, xmm3
        vaddps  xmm5, xmm5, xmm6
        vmovaps xmmword ptr [rsp + 4*rax], xmm5
        add     rax, 4
        cmp     rax, 16
        jne     $LL0
        vmovaps ymm0, ymmword ptr [rsp]
        vmovups xmmword ptr [rdi], xmm0
        vmovaps xmm0, xmmword ptr [rsp + 16]
        vmovups xmmword ptr [rdi + 16], xmm0
        vmovaps ymm0, ymmword ptr [rsp + 32]
        vmovups xmmword ptr [rdi + 32], xmm0
        vmovaps xmm0, xmmword ptr [rsp + 48]
        vmovups xmmword ptr [rdi + 48], xmm0
        mov     rsp, rbp
        pop     rbp
        vzeroupper
        ret
$mat4x4_multiply_inplace_assign_col_major:
        push    rbp
        mov     rbp, rsp
        and     rsp, -32
        sub     rsp, 96
        vmovss  xmm0, dword ptr [rsi]           
        vmovss  xmm1, dword ptr [rsi + 4]       
        vmovss  xmm2, dword ptr [rsi + 8]       
        vmovss  xmm3, dword ptr [rsi + 12]      
        vinsertps       xmm0, xmm0, dword ptr [rsi + 16], 16 
        vinsertps       xmm0, xmm0, dword ptr [rsi + 32], 32 
        vinsertps       xmm0, xmm0, dword ptr [rsi + 48], 48 
        vinsertps       xmm1, xmm1, dword ptr [rsi + 20], 16 
        vinsertps       xmm1, xmm1, dword ptr [rsi + 36], 32 
        vinsertps       xmm1, xmm1, dword ptr [rsi + 52], 48 
        vinsertps       xmm2, xmm2, dword ptr [rsi + 24], 16 
        vinsertps       xmm2, xmm2, dword ptr [rsi + 40], 32 
        vinsertps       xmm2, xmm2, dword ptr [rsi + 56], 48 
        vinsertps       xmm3, xmm3, dword ptr [rsi + 28], 16 
        vinsertps       xmm3, xmm3, dword ptr [rsi + 44], 32 
        vinsertps       xmm3, xmm3, dword ptr [rsi + 60], 48 
        xor     eax, eax
        vxorps  xmm4, xmm4, xmm4
$LL1:
        vmovaps xmmword ptr [rsp + 4*rax], xmm4
        vbroadcastss    xmm5, dword ptr [rdi + rax + 16]
        vmulps  xmm5, xmm5, xmm1
        vbroadcastss    xmm6, dword ptr [rdi + rax]
        vmulps  xmm6, xmm6, xmm0
        vaddps  xmm6, xmm6, xmm4
        vaddps  xmm5, xmm6, xmm5
        vbroadcastss    xmm6, dword ptr [rdi + rax + 32]
        vmulps  xmm6, xmm6, xmm2
        vaddps  xmm5, xmm5, xmm6
        vbroadcastss    xmm6, dword ptr [rdi + rax + 48]
        vmulps  xmm6, xmm6, xmm3
        vaddps  xmm5, xmm5, xmm6
        vmovaps xmmword ptr [rsp + 4*rax], xmm5
        add     rax, 4
        cmp     rax, 16
        jne     $LL1
        vmovaps ymm0, ymmword ptr [rsp]
        vmovups xmmword ptr [rdi], xmm0
        vmovaps xmm0, xmmword ptr [rsp + 16]
        vmovups xmmword ptr [rdi + 16], xmm0
        vmovaps ymm0, ymmword ptr [rsp + 32]
        vmovups xmmword ptr [rdi + 32], xmm0
        vmovaps xmm0, xmmword ptr [rsp + 48]
        vmovups xmmword ptr [rdi + 48], xmm0
        mov     rsp, rbp
        pop     rbp
        vzeroupper
        ret

tempest_math_aligned_inplace_mul_mat4_mat4 ENDP

END