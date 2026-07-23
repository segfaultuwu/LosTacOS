BITS 64

section .text


global irq0
global irq1_handler

extern timer_irq
extern keyboard_irq


%macro PUSH_REGS 0

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

%endmacro


%macro POP_REGS 0

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

%endmacro



irq0:

    push 0
    push 32

    PUSH_REGS

    mov rdi,rsp
    call timer_irq

    mov rsp,rax

    POP_REGS

    add rsp,16

    iretq



irq1_handler:

    push 0
    push 33

    PUSH_REGS

    mov rdi,rsp
    call keyboard_irq

    mov rsp,rax

    POP_REGS

    add rsp,16

    iretq
