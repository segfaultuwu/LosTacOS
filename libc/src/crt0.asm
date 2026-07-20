global _start
extern main
extern exit

section .text

_start:

    call main

    mov rdi, rax
    call exit
