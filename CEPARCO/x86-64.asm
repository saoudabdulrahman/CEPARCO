; Scalar vector triad, no SIMD:  A[i] = B[i] + C[i] * D[i]
; Assemble with NASM:  nasm -f win64 x86-64.asm
;
; C prototype:
;   void asm_x86_64(unsigned long long n, float* A, const float* B,
;                   const float* C, const float* D);
;
; Windows x64 calling convention: n is in rcx, A in rdx, B in r8, C in r9.
; D is the fifth argument, so it's passed on the stack and sits at [rsp+40]
; when we're entered.
;
; We only touch volatile registers (rax, r10, xmm0), so there's nothing to
; save or restore on the way out.
default rel
bits 64

section .text
global asm_x86_64

asm_x86_64:
    mov     r10, [rsp+40]            ; grab the pointer to D
    xor     rax, rax                 ; i = 0
    test    rcx, rcx
    jz      .done                    ; n is 0, nothing to do

.loop:
    movss   xmm0, dword [r9+rax*4]   ; C[i]
    mulss   xmm0, dword [r10+rax*4]  ; C[i] * D[i]
    addss   xmm0, dword [r8+rax*4]   ; + B[i]
    movss   dword [rdx+rax*4], xmm0  ; store into A[i]
    inc     rax
    cmp     rax, rcx
    jb      .loop                    ; loop while i < n

.done:
    ret
