BITS 64

global _start

section .text

_start:
    mov rax,1
    mov rbx,1
    mov rcx,msg
    mov rdx,5
    int 0x80

.loop:
    jmp .loop


section .data

msg:
    db "HELLO",10
