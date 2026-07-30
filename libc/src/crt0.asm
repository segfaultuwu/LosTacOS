global _start
extern main
extern exit

section .text

_start:
    ; save initial stack
    mov r12, rsp

    ; argc
    mov rdi, [r12]

    ; argv
    lea rsi, [r12 + 8]

    ; envp = argv + argc + 1
    mov rdx, rsi
    mov rax, [r12]
    inc rax
    shl rax, 3
    add rdx, rax
    add rdx, 8

    ; SysV ABI stack alignment
    and rsp, -16

    call main

    mov rdi, rax
    call exit

.hang:
    jmp .hang
