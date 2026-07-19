BITS 64

SYS_WRITE equ 1
SYS_EXIT  equ 3

STDOUT equ 1

section .text
global _start

_start:
    mov rax, SYS_WRITE
    mov rbx, STDOUT
    mov rcx, msg
    mov rdx, msg_len
    int 0x80

    mov rax, SYS_EXIT
    xor rbx, rbx
    int 0x80

.hang:
    jmp .hang

section .data
msg: db "Hello from /bin/hello!", 10
msg_len equ $ - msg
