# Signature:
# void tempest_math_aligned_assign_mul_mat4_mat4(const float* lhs, const float* rhs, float* result);
#
# Requirements:
# - The result pointer must be aligned to 16 bytes.
# - The lhs and rhs pointers must be aligned to 16 bytes.
# - The data pointers are assumed to be valid.
# - The data pointers represent column major matrices

    .file "mul_assign_mat4_mat4.s"
    .intel_syntax noprefix
    .text
.Ltext0:
    .p2align 4
    .globl tempest_math_aligned_assign_mul_mat4_mat4
    .type  tempest_math_aligned_assign_mul_mat4_mat4, @function
.LCPI0_0:
    .quad   32                              # 0x20
    .quad   48                              # 0x30
.LCPI0_1:
    .byte   0                               # 0x0
    .byte   0                               # 0x0
    .byte   0                               # 0x0
    .byte   0                               # 0x0
    .byte   0                               # 0x0
    .byte   0                               # 0x0
    .byte   0                               # 0x0
    .byte   0                               # 0x0
    .byte   16                              # 0x10
    .byte   0                               # 0x0
    .byte   0                               # 0x0
    .byte   0                               # 0x0
    .byte   0                               # 0x0
    .byte   0                               # 0x0
    .byte   0                               # 0x0
    .byte   0                               # 0x0
tempest_math_aligned_assign_mul_mat4_mat4:
    push    rbp
    mov     rbp, rsp
    and     rsp, -32
    sub     rsp, 96
    mov     rax, rsp
    vmovq   xmm0, rax
    vpshufd xmm0, xmm0, 68                  # xmm0 = xmm0[0,1,0,1]
    vinsertf128     ymm0, ymm0, xmm0, 1
    xor     eax, eax
    vextractf128    xmm1, ymm0, 1
    vmovdqa xmm2, xmmword ptr [rip + .LCPI0_0] # xmm2 = [32,48]
    vmovdqa xmm3, xmmword ptr [rip + .LCPI0_1] # xmm3 = [0,0,0,0,0,0,0,0,16,0,0,0,0,0,0,0]
.LBB0_1:                                # =>This Inner Loop Header: Depth=1
    vmovd   xmm4, eax
    vpshufd xmm4, xmm4, 0                   # xmm4 = xmm4[0,0,0,0]
    vpslld  xmm4, xmm4, 2
    vpmovzxdq       xmm4, xmm4              # xmm4 = xmm4[0],zero,xmm4[1],zero
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
    vmovss  xmm5, dword ptr [rsi]           # xmm5 = mem[0],zero,zero,zero
    vinsertps       xmm5, xmm5, dword ptr [rsi + 16], 16 # xmm5 = xmm5[0],mem[0],xmm5[2,3]
    vinsertps       xmm5, xmm5, dword ptr [rsi + 32], 32 # xmm5 = xmm5[0,1],mem[0],xmm5[3]
    vinsertps       xmm5, xmm5, dword ptr [rsi + 48], 48 # xmm5 = xmm5[0,1,2],mem[0]
    vmovss  xmm6, dword ptr [rcx]           # xmm6 = mem[0],zero,zero,zero
    vinsertps       xmm6, xmm6, dword ptr [rdx], 28 # xmm6 = xmm6[0],mem[0],zero,zero
    vinsertps       xmm6, xmm6, dword ptr [r8], 40 # xmm6 = xmm6[0,1],mem[0],zero
    vmulps  xmm4, xmm4, xmm5
    vaddps  xmm4, xmm4, xmm6
    vmovss  dword ptr [rcx], xmm4
    vextractps      dword ptr [rdx], xmm4, 1
    vextractps      dword ptr [r8], xmm4, 2
    vshufps xmm5, xmm4, xmm4, 255           # xmm5 = xmm4[3,3,3,3]
    vmovss  dword ptr [r9], xmm5
    vbroadcastss    ymm5, dword ptr [rdi + 4*rax + 16]
    vmovss  xmm6, dword ptr [rsi + 4]       # xmm6 = mem[0],zero,zero,zero
    vinsertps       xmm6, xmm6, dword ptr [rsi + 20], 16 # xmm6 = xmm6[0],mem[0],xmm6[2,3]
    vinsertps       xmm6, xmm6, dword ptr [rsi + 36], 32 # xmm6 = xmm6[0,1],mem[0],xmm6[3]
    vinsertps       xmm6, xmm6, dword ptr [rsi + 52], 48 # xmm6 = xmm6[0,1,2],mem[0]
    vmovss  xmm7, dword ptr [rcx]           # xmm7 = mem[0],zero,zero,zero
    vinsertps       xmm7, xmm7, dword ptr [rdx], 16 # xmm7 = xmm7[0],mem[0],xmm7[2,3]
    vinsertps       xmm7, xmm7, dword ptr [r8], 32 # xmm7 = xmm7[0,1],mem[0],xmm7[3]
    vblendps        xmm4, xmm7, xmm4, 8             # xmm4 = xmm7[0,1,2],xmm4[3]
    vmulps  xmm5, xmm5, xmm6
    vaddps  xmm4, xmm5, xmm4
    vmovss  dword ptr [rcx], xmm4
    vextractps      dword ptr [rdx], xmm4, 1
    vextractps      dword ptr [r8], xmm4, 2
    vshufps xmm5, xmm4, xmm4, 255           # xmm5 = xmm4[3,3,3,3]
    vmovss  dword ptr [r9], xmm5
    vbroadcastss    ymm5, dword ptr [rdi + 4*rax + 32]
    vmovss  xmm6, dword ptr [rsi + 8]       # xmm6 = mem[0],zero,zero,zero
    vinsertps       xmm6, xmm6, dword ptr [rsi + 24], 16 # xmm6 = xmm6[0],mem[0],xmm6[2,3]
    vinsertps       xmm6, xmm6, dword ptr [rsi + 40], 32 # xmm6 = xmm6[0,1],mem[0],xmm6[3]
    vinsertps       xmm6, xmm6, dword ptr [rsi + 56], 48 # xmm6 = xmm6[0,1,2],mem[0]
    vmovss  xmm7, dword ptr [rcx]           # xmm7 = mem[0],zero,zero,zero
    vinsertps       xmm7, xmm7, dword ptr [rdx], 16 # xmm7 = xmm7[0],mem[0],xmm7[2,3]
    vinsertps       xmm7, xmm7, dword ptr [r8], 32 # xmm7 = xmm7[0,1],mem[0],xmm7[3]
    vblendps        xmm4, xmm7, xmm4, 8             # xmm4 = xmm7[0,1,2],xmm4[3]
    vmulps  xmm5, xmm5, xmm6
    vaddps  xmm4, xmm5, xmm4
    vmovss  dword ptr [rcx], xmm4
    vextractps      dword ptr [rdx], xmm4, 1
    vextractps      dword ptr [r8], xmm4, 2
    vshufps xmm5, xmm4, xmm4, 255           # xmm5 = xmm4[3,3,3,3]
    vmovss  dword ptr [r9], xmm5
    vbroadcastss    ymm5, dword ptr [rdi + 4*rax + 48]
    vmovss  xmm6, dword ptr [rsi + 12]      # xmm6 = mem[0],zero,zero,zero
    vinsertps       xmm6, xmm6, dword ptr [rsi + 28], 16 # xmm6 = xmm6[0],mem[0],xmm6[2,3]
    vinsertps       xmm6, xmm6, dword ptr [rsi + 44], 32 # xmm6 = xmm6[0,1],mem[0],xmm6[3]
    vinsertps       xmm6, xmm6, dword ptr [rsi + 60], 48 # xmm6 = xmm6[0,1,2],mem[0]
    vmovss  xmm7, dword ptr [rcx]           # xmm7 = mem[0],zero,zero,zero
    vinsertps       xmm7, xmm7, dword ptr [rdx], 16 # xmm7 = xmm7[0],mem[0],xmm7[2,3]
    vinsertps       xmm7, xmm7, dword ptr [r8], 32 # xmm7 = xmm7[0,1],mem[0],xmm7[3]
    vblendps        xmm4, xmm7, xmm4, 8             # xmm4 = xmm7[0,1,2],xmm4[3]
    vmulps  xmm5, xmm5, xmm6
    vaddps  xmm4, xmm5, xmm4
    vmovss  dword ptr [rcx], xmm4
    vextractps      dword ptr [rdx], xmm4, 1
    vextractps      dword ptr [r8], xmm4, 2
    vextractps      dword ptr [r9], xmm4, 3
    inc     rax
    cmp     rax, 4
    jne     .LBB0_1
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
.LCPI1_0:
    .quad   32                              # 0x20
    .quad   48                              # 0x30
.LCPI1_1:
    .byte   0                               # 0x0
    .byte   0                               # 0x0
    .byte   0                               # 0x0
    .byte   0                               # 0x0
    .byte   0                               # 0x0
    .byte   0                               # 0x0
    .byte   0                               # 0x0
    .byte   0                               # 0x0
    .byte   16                              # 0x10
    .byte   0                               # 0x0
    .byte   0                               # 0x0
    .byte   0                               # 0x0
    .byte   0                               # 0x0
    .byte   0                               # 0x0
    .byte   0                               # 0x0
    .byte   0                               # 0x0
mat4x4_multiply_inplace_col_major:      # @mat4x4_multiply_inplace_col_major
    push    rbp
    mov     rbp, rsp
    and     rsp, -32
    sub     rsp, 96
    mov     rax, rsp
    vmovq   xmm0, rax
    vpshufd xmm0, xmm0, 68                  # xmm0 = xmm0[0,1,0,1]
    vinsertf128     ymm0, ymm0, xmm0, 1
    xor     eax, eax
    vextractf128    xmm1, ymm0, 1
    vmovdqa xmm2, xmmword ptr [rip + .LCPI1_0] # xmm2 = [32,48]
    vmovdqa xmm3, xmmword ptr [rip + .LCPI1_1] # xmm3 = [0,0,0,0,0,0,0,0,16,0,0,0,0,0,0,0]
.LBB1_1:                                # =>This Inner Loop Header: Depth=1
    vmovd   xmm4, eax
    vpshufd xmm4, xmm4, 0                   # xmm4 = xmm4[0,0,0,0]
    vpslld  xmm4, xmm4, 2
    vpmovzxdq       xmm4, xmm4              # xmm4 = xmm4[0],zero,xmm4[1],zero
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
    vmovss  xmm5, dword ptr [rsi]           # xmm5 = mem[0],zero,zero,zero
    vinsertps       xmm5, xmm5, dword ptr [rsi + 16], 16 # xmm5 = xmm5[0],mem[0],xmm5[2,3]
    vinsertps       xmm5, xmm5, dword ptr [rsi + 32], 32 # xmm5 = xmm5[0,1],mem[0],xmm5[3]
    vinsertps       xmm5, xmm5, dword ptr [rsi + 48], 48 # xmm5 = xmm5[0,1,2],mem[0]
    vmovss  xmm6, dword ptr [rcx]           # xmm6 = mem[0],zero,zero,zero
    vinsertps       xmm6, xmm6, dword ptr [rdx], 28 # xmm6 = xmm6[0],mem[0],zero,zero
    vinsertps       xmm6, xmm6, dword ptr [r8], 40 # xmm6 = xmm6[0,1],mem[0],zero
    vmulps  xmm4, xmm4, xmm5
    vaddps  xmm4, xmm4, xmm6
    vmovss  dword ptr [rcx], xmm4
    vextractps      dword ptr [rdx], xmm4, 1
    vextractps      dword ptr [r8], xmm4, 2
    vshufps xmm5, xmm4, xmm4, 255           # xmm5 = xmm4[3,3,3,3]
    vmovss  dword ptr [r9], xmm5
    vbroadcastss    ymm5, dword ptr [rdi + 4*rax + 16]
    vmovss  xmm6, dword ptr [rsi + 4]       # xmm6 = mem[0],zero,zero,zero
    vinsertps       xmm6, xmm6, dword ptr [rsi + 20], 16 # xmm6 = xmm6[0],mem[0],xmm6[2,3]
    vinsertps       xmm6, xmm6, dword ptr [rsi + 36], 32 # xmm6 = xmm6[0,1],mem[0],xmm6[3]
    vinsertps       xmm6, xmm6, dword ptr [rsi + 52], 48 # xmm6 = xmm6[0,1,2],mem[0]
    vmovss  xmm7, dword ptr [rcx]           # xmm7 = mem[0],zero,zero,zero
    vinsertps       xmm7, xmm7, dword ptr [rdx], 16 # xmm7 = xmm7[0],mem[0],xmm7[2,3]
    vinsertps       xmm7, xmm7, dword ptr [r8], 32 # xmm7 = xmm7[0,1],mem[0],xmm7[3]
    vblendps        xmm4, xmm7, xmm4, 8             # xmm4 = xmm7[0,1,2],xmm4[3]
    vmulps  xmm5, xmm5, xmm6
    vaddps  xmm4, xmm5, xmm4
    vmovss  dword ptr [rcx], xmm4
    vextractps      dword ptr [rdx], xmm4, 1
    vextractps      dword ptr [r8], xmm4, 2
    vshufps xmm5, xmm4, xmm4, 255           # xmm5 = xmm4[3,3,3,3]
    vmovss  dword ptr [r9], xmm5
    vbroadcastss    ymm5, dword ptr [rdi + 4*rax + 32]
    vmovss  xmm6, dword ptr [rsi + 8]       # xmm6 = mem[0],zero,zero,zero
    vinsertps       xmm6, xmm6, dword ptr [rsi + 24], 16 # xmm6 = xmm6[0],mem[0],xmm6[2,3]
    vinsertps       xmm6, xmm6, dword ptr [rsi + 40], 32 # xmm6 = xmm6[0,1],mem[0],xmm6[3]
    vinsertps       xmm6, xmm6, dword ptr [rsi + 56], 48 # xmm6 = xmm6[0,1,2],mem[0]
    vmovss  xmm7, dword ptr [rcx]           # xmm7 = mem[0],zero,zero,zero
    vinsertps       xmm7, xmm7, dword ptr [rdx], 16 # xmm7 = xmm7[0],mem[0],xmm7[2,3]
    vinsertps       xmm7, xmm7, dword ptr [r8], 32 # xmm7 = xmm7[0,1],mem[0],xmm7[3]
    vblendps        xmm4, xmm7, xmm4, 8             # xmm4 = xmm7[0,1,2],xmm4[3]
    vmulps  xmm5, xmm5, xmm6
    vaddps  xmm4, xmm5, xmm4
    vmovss  dword ptr [rcx], xmm4
    vextractps      dword ptr [rdx], xmm4, 1
    vextractps      dword ptr [r8], xmm4, 2
    vshufps xmm5, xmm4, xmm4, 255           # xmm5 = xmm4[3,3,3,3]
    vmovss  dword ptr [r9], xmm5
    vbroadcastss    ymm5, dword ptr [rdi + 4*rax + 48]
    vmovss  xmm6, dword ptr [rsi + 12]      # xmm6 = mem[0],zero,zero,zero
    vinsertps       xmm6, xmm6, dword ptr [rsi + 28], 16 # xmm6 = xmm6[0],mem[0],xmm6[2,3]
    vinsertps       xmm6, xmm6, dword ptr [rsi + 44], 32 # xmm6 = xmm6[0,1],mem[0],xmm6[3]
    vinsertps       xmm6, xmm6, dword ptr [rsi + 60], 48 # xmm6 = xmm6[0,1,2],mem[0]
    vmovss  xmm7, dword ptr [rcx]           # xmm7 = mem[0],zero,zero,zero
    vinsertps       xmm7, xmm7, dword ptr [rdx], 16 # xmm7 = xmm7[0],mem[0],xmm7[2,3]
    vinsertps       xmm7, xmm7, dword ptr [r8], 32 # xmm7 = xmm7[0,1],mem[0],xmm7[3]
    vblendps        xmm4, xmm7, xmm4, 8             # xmm4 = xmm7[0,1,2],xmm4[3]
    vmulps  xmm5, xmm5, xmm6
    vaddps  xmm4, xmm5, xmm4
    vmovss  dword ptr [rcx], xmm4
    vextractps      dword ptr [rdx], xmm4, 1
    vextractps      dword ptr [r8], xmm4, 2
    vextractps      dword ptr [r9], xmm4, 3
    inc     rax
    cmp     rax, 4
    jne     .LBB1_1
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