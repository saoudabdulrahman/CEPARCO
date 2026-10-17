; 128-bit AVX vector triad:  A[i] = B[i] + C[i] * D[i]
; Assemble with NASM:  nasm -f win64 simdxmm.asm
;
; C prototype:
;   void asm_simd_xmm(unsigned long long n, float* A, const float* B,
;                     const float* C, const float* D);
;
; Same argument layout as the scalar version: n in rcx, A in rdx, B in r8,
; C in r9, D on the stack at [rsp+40].
;
; The main loop does 4 floats per pass. Whatever is left over (n mod 4) gets
; handled one float at a time at the bottom.
;
; Loads and stores use vmovups, so the arrays don't need any particular
; alignment. We do a separate multiply and add rather than FMA, so the
; rounding matches the scalar and YMM versions exactly.
;
; Only xmm0 and the volatile integer registers are used. VEX-encoded ops
; that write an XMM register also clear the upper half of the matching YMM,
; so there's no dirty upper state to clean up and no vzeroupper is needed.
default rel
bits 64

section .text
global asm_simd_xmm

asm_simd_xmm:
    mov     r10, [rsp+40]            ; pointer to D
    xor     rax, rax                 ; i = 0
    mov     r11, rcx
    and     r11, -4                  ; r11 = n rounded down to a multiple of 4
    jz      .tail                    ; n < 4, so skip straight to the leftovers

.vec_loop:
    vmovups xmm0, [r9+rax*4]         ; load C[i..i+3]
    vmulps  xmm0, xmm0, [r10+rax*4]  ; C * D, four at once
    vaddps  xmm0, xmm0, [r8+rax*4]   ; + B
    vmovups [rdx+rax*4], xmm0        ; store A[i..i+3]
    add     rax, 4
    cmp     rax, r11
    jb      .vec_loop

.tail:                               ; the last n mod 4 elements, one at a time
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
    ret
