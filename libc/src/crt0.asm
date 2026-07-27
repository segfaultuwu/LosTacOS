global _start
extern main
extern exit

section .text

_start:
    mov rdi, [rsp]        ; argc
    lea rsi, [rsp+8]      ; argv
    mov rdx, rsi

    mov rax, [rsp]
    inc rax
    shl rax, 3
    add rdx, rax
    add rdx, 8            ; envp

    call main

    mov rdi, rax
    call exit
