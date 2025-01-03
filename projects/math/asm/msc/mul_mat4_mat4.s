.data
    load_offsets    DWORD 0, 16, 32, 48

.code
public tempest_math_aligned_mul_mat4_mat4

$lhs = 8
$rhs = 16
$out = 24

tempest_math_aligned_mul_mat4_mat4 PROC
    push r15
    push r14
    push r12
    push rbx

    push rdi
    push rsi
    push rdx

    ; copy RCX -> RDI
    ; copy RDX -> RSI
    ; copy R8 -> RDX

    mov rdi, rcx
    mov rsi, rdx
    mov rdx, r8

    xor eax, eax
    vmovdqa xmm0, xmmword ptr [OFFSET load_offsets]
$LL1:
    vmovd   xmm1, eax
    vpslld  xmm1, xmm1, 2
    vpshufd xmm2, xmm1, 0
    vpor    xmm2, xmm2, xmm0
    vmovd   ecx, xmm1
    mov     dword ptr [rdx + rcx], 0
    vpextrd r8d, xmm2, 1
    mov     dword ptr [rdx + r8], 0
    vpextrd r9d, xmm2, 2
    vpextrd r10d, xmm2, 3
    mov     dword ptr [rdx + r9], 0
    mov     dword ptr [rdx + r10], 0
    vpxor   xmm1, xmm1, xmm1
    xor     r11d, r11d
$LL2:
    vbroadcastss ymm2, dword ptr [rdi + 4 * r11]
    vmovd xmm3, r11d
    vpshufd xmm3, xmm3, 0
    vpor xmm3, xmm3, xmm0
    vpextrd ebx, xmm3, 1
    vpextrd r14d, xmm3, 2
    mov r15d, r11d
    vpextrd r12d, xmm3, 3
    vmovss xmm3, dword ptr [rsi + r15] 
    vinsertps xmm3, xmm3, dword ptr [rsi + rbx], 16
    vinsertps xmm3, xmm3, dword ptr [rsi + r14], 32
    vinsertps xmm3, xmm3, dword ptr [rsi + r12], 48
    vmovss xmm4, dword ptr [rdx + rcx]
    vinsertps xmm4, xmm4, dword ptr [rdx + r8], 16
    vinsertps xmm4, xmm4, dword ptr [rdx + r9], 32
    vinsertps xmm1, xmm4, xmm1, 48
    vmulps xmm2, xmm2, xmm3
    vaddps xmm1, xmm2, xmm1
    vmovss dword ptr [rdx + rcx], xmm1
    vextractps dword ptr [rdx + r8], xmm1, 1
    vextractps dword ptr [rdx + r9], xmm1, 2
    vshufps xmm1, xmm1, xmm1, 255
    vmovss dword ptr [rdx + r10], xmm1
    add r11, 4
    cmp r11, 16
    jne $LL2
    inc rax
    add rdi, 4
    cmp rax, 4
    jne $LL1

    pop rdx
    pop rsi
    pop rdi

    pop rbx
    pop r12
    pop r14
    pop r15
    ret

tempest_math_aligned_mul_mat4_mat4 ENDP

END
