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
    mov     rax, rsp

    push    rdi
    push    rsi

    ; copy RCX -> RDI
    ; copy RDX -> RSI
    
    mov rdi, rcx
    mov rsi, rdx

    vmovq   xmm0, rax
    vpshufd xmm0, xmm0, 68                  
    vinsertf128     ymm0, ymm0, xmm0, 1
    xor     eax, eax
    vextractf128    xmm1, ymm0, 1
    vmovdqa xmm2, xmmword ptr [OFFSET LCPI0_0]
    vmovdqa xmm3, xmmword ptr [OFFSET LCPI0_1]
    xor     eax, eax
    vxorps  xmm4, xmm4, xmm4
$LL0:
    vmovd   xmm4, eax
    vpshufd xmm4, xmm4, 0                  
    vpslld  xmm4, xmm4, 2
    vpmovzxdq       xmm4, xmm4             
    vpor    xmm5, xmm1, xmm4
    vpaddq  xmm5, xmm5, xmm2
    vpor    xmm4, xmm0, xmm4
    vpaddq  xmm4, xmm4, xmm3
    vmovq   rcx, xmm4
    mov     dword ptr [rcx], 0
    vpextrq rdx, xmm4, 1
    mov     dword ptr [rdx], 0
    vmovq   r8, xmm5
    vpextrq r9, xmm5, 1
    mov     dword ptr [r8], 0
    mov     dword ptr [r9], 0
    vbroadcastss    ymm4, dword ptr [rdi + 4*rax]
    vmovss  xmm5, dword ptr [rsi]           
    vinsertps       xmm5, xmm5, dword ptr [rsi + 16], 16 
    vinsertps       xmm5, xmm5, dword ptr [rsi + 32], 32 
    vinsertps       xmm5, xmm5, dword ptr [rsi + 48], 48 
    vmovss  xmm6, dword ptr [rcx]           
    vinsertps       xmm6, xmm6, dword ptr [rdx], 28
    vinsertps       xmm6, xmm6, dword ptr [r8], 40
    vmulps  xmm4, xmm4, xmm5
    vaddps  xmm4, xmm4, xmm6
    vmovss  dword ptr [rcx], xmm4
    vextractps      dword ptr [rdx], xmm4, 1
    vextractps      dword ptr [r8], xmm4, 2
    vshufps xmm5, xmm4, xmm4, 255           
    vmovss  dword ptr [r9], xmm5
    vbroadcastss    ymm5, dword ptr [rdi + 4*rax + 16]
    vmovss  xmm6, dword ptr [rsi + 4]       
    vinsertps       xmm6, xmm6, dword ptr [rsi + 20], 16 
    vinsertps       xmm6, xmm6, dword ptr [rsi + 36], 32 
    vinsertps       xmm6, xmm6, dword ptr [rsi + 52], 48 
    vmovss  xmm7, dword ptr [rcx]           
    vinsertps       xmm7, xmm7, dword ptr [rdx], 16
    vinsertps       xmm7, xmm7, dword ptr [r8], 32 
    vblendps        xmm4, xmm7, xmm4, 8            
    vmulps  xmm5, xmm5, xmm6
    vaddps  xmm4, xmm5, xmm4
    vmovss  dword ptr [rcx], xmm4
    vextractps      dword ptr [rdx], xmm4, 1
    vextractps      dword ptr [r8], xmm4, 2
    vshufps xmm5, xmm4, xmm4, 255          
    vmovss  dword ptr [r9], xmm5
    vbroadcastss    ymm5, dword ptr [rdi + 4*rax + 32]
    vmovss  xmm6, dword ptr [rsi + 8]      
    vinsertps       xmm6, xmm6, dword ptr [rsi + 24], 16
    vinsertps       xmm6, xmm6, dword ptr [rsi + 40], 32
    vinsertps       xmm6, xmm6, dword ptr [rsi + 56], 48
    vmovss  xmm7, dword ptr [rcx]          
    vinsertps       xmm7, xmm7, dword ptr [rdx], 16
    vinsertps       xmm7, xmm7, dword ptr [r8], 32 
    vblendps        xmm4, xmm7, xmm4, 8            
    vmulps  xmm5, xmm5, xmm6
    vaddps  xmm4, xmm5, xmm4
    vmovss  dword ptr [rcx], xmm4
    vextractps      dword ptr [rdx], xmm4, 1
    vextractps      dword ptr [r8], xmm4, 2
    vshufps xmm5, xmm4, xmm4, 255          
    vmovss  dword ptr [r9], xmm5
    vbroadcastss    ymm5, dword ptr [rdi + 4*rax + 48]
    vmovss  xmm6, dword ptr [rsi + 12]    
    vinsertps       xmm6, xmm6, dword ptr [rsi + 28], 16 
    vinsertps       xmm6, xmm6, dword ptr [rsi + 44], 32 
    vinsertps       xmm6, xmm6, dword ptr [rsi + 60], 48 
    vmovss  xmm7, dword ptr [rcx]           
    vinsertps       xmm7, xmm7, dword ptr [rdx], 16
    vinsertps       xmm7, xmm7, dword ptr [r8], 32
    vblendps        xmm4, xmm7, xmm4, 8            
    vmulps  xmm5, xmm5, xmm6
    vaddps  xmm4, xmm5, xmm4
    vmovss  dword ptr [rcx], xmm4
    vextractps      dword ptr [rdx], xmm4, 1
    vextractps      dword ptr [r8], xmm4, 2
    vextractps      dword ptr [r9], xmm4, 3
    inc     rax
    cmp     rax, 4
    jne     $LL0
    vmovaps ymm0, ymmword ptr [rsp]
    vmovups xmmword ptr [rdi], xmm0
    vmovaps xmm0, xmmword ptr [rsp + 16]
    vmovups xmmword ptr [rdi + 16], xmm0
    vmovaps ymm0, ymmword ptr [rsp + 32]
    vmovups xmmword ptr [rdi + 32], xmm0
    vmovaps xmm0, xmmword ptr [rsp + 48]
    vmovups xmmword ptr [rdi + 48], xmm0
    
    pop     rsi
    pop     rdi
    mov     rsp, rbp
    pop     rbp
    vzeroupper
    ret

tempest_math_aligned_assign_mul_mat4_mat4 ENDP

END