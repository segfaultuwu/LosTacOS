BITS 64

; Syscall numbers - must match include/LTOS/syscall.hpp
SYS_WRITE equ 1
SYS_EXIT  equ 3

; File descriptors
STDOUT equ 1

section .text
global _start

_start:
    ; Write message to stdout
    mov rax, SYS_WRITE
    mov rbx, STDOUT
    mov rcx, msg
    mov rdx, msg_len
    int 0x80

    ; Exit
    mov rax, SYS_EXIT
    xor rbx, rbx        ; exit code 0
    int 0x80

    ; Should never reach here, but just in case
.hang:
    jmp .hang

section .data
msg: db "Hello from /bin/hello!", 10
msg_len equ $ - msg
