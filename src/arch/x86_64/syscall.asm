BITS 64

section .text

global isr128

extern syscall_handler


isr128:

    cli


    push rax
    push rbx
    push rcx
    push rdx

    push rsi
    push rdi

    push rbp

    push r8
    push r9
    push r10
    push r11

    push r12
    push r13
    push r14
    push r15


    sti


    ; syscall ABI:
    ; rax = number
    ; rbx = arg1
    ; rcx = arg2
    ; rdx = arg3


    mov rdi,[rsp+112]
    mov rsi,[rsp+104]
    mov rdx,[rsp+96]
    mov rcx,[rsp+88]


    call syscall_handler


    mov [rsp+112],rax


    pop r15
    pop r14
    pop r13
    pop r12

    pop r11
    pop r10
    pop r9
    pop r8

    pop rbp

    pop rdi
    pop rsi

    pop rdx
    pop rcx
    pop rbx
    pop rax


    iretq
