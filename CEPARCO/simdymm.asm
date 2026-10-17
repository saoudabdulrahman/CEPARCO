; 256-bit AVX2 vector triad:  A[i] = B[i] + C[i] * D[i]
; Assemble with NASM:  nasm -f win64 simdymm.asm
;
; C prototype:
;   void asm_simd_ymm(unsigned long long n, float* A, const float* B,
;                     const float* C, const float* D);
;
; Same argument layout as the other kernels: n in rcx, A in rdx, B in r8,
; C in r9, D on the stack at [rsp+40].
;
; The main loop does 8 floats per pass. The n mod 8 leftovers are handled
; one at a time at the bottom, using VEX-encoded scalar ops.
;
; Loads and stores use vmovups (no alignment needed). Multiply and add are
; separate, not FMA, so the results match the XMM and scalar versions.
;
; We use ymm0 plus the volatile integer registers. Before returning we run
; vzeroupper, so the caller doesn't pay the AVX-to-SSE transition penalty.
default rel
bits 64

section .text
global asm_simd_ymm

asm_simd_ymm:
    mov     r10, [rsp+40]            ; pointer to D
    xor     rax, rax                 ; i = 0
    mov     r11, rcx
    and     r11, -8                  ; r11 = n rounded down to a multiple of 8
    jz      .tail                    ; n < 8, so skip straight to the leftovers

.vec_loop:
    vmovups ymm0, [r9+rax*4]         ; load C[i..i+7]
    vmulps  ymm0, ymm0, [r10+rax*4]  ; C * D, eight at once
    vaddps  ymm0, ymm0, [r8+rax*4]   ; + B
    vmovups [rdx+rax*4], ymm0        ; store A[i..i+7]
    add     rax, 8
    cmp     rax, r11
    jb      .vec_loop

.tail:                               ; the last n mod 8 elements, one at a time
    cmp     rax, rcx
    jae     .done

.tail_loop:
    vmovss  xmm0, dword [r9+rax*4]
    vmulss  xmm0, xmm0, dword [r10+rax*4]
    vaddss  xmm0, xmm0, dword [r8+rax*4]
    vmovss  dword [rdx+rax*4], xmm0
    inc     rax
    cmp     rax, rcx
    jb      .tail_loop

.done:
    vzeroupper                       ; clear the upper YMM state before returning
    ret
