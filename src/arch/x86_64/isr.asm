BITS 64


global lidt


section .text


lidt:
    lidt [rdi]
    ret



; =====================
; IRQ0 PIT
; =====================

global irq0
extern timer_irq


irq0:

    push 0          ; error
    push 32         ; vector

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


    mov rdi, rsp

    call timer_irq


    mov rsp, rax


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


    add rsp,16

    iretq



; =====================
; IRQ1 KEYBOARD
; =====================

global irq1_handler

extern keyboard_irq


irq1_handler:

    push 0
    push 33


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


    mov rdi,rsp

    call keyboard_irq


    mov rsp,rax


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


    add rsp,16

    iretq



; =====================
; Exceptions
; =====================


extern unhandled_interrupt


isr_stub_common:


    cli


    ; after registers are not pushed yet:
    ; rsp:
    ; 0 error
    ; 8 vector
    ; 16 rip


    mov rdi,[rsp+8]
    mov rsi,[rsp]

    mov rdx,[rsp+16]


    cmp rdi,14
    jne .call


    mov rax,cr2
    mov rsi,rax


.call:

    call unhandled_interrupt


.hang:

    hlt
    jmp .hang



%macro ISR_ERR 1

global isr_stub_%1

isr_stub_%1:

    cli

    push qword %1

    jmp isr_stub_common

%endmacro



%macro ISR_NOERR 1

global isr_stub_%1

isr_stub_%1:

    cli

    push qword 0
    push qword %1

    jmp isr_stub_common

%endmacro



%assign i 0

%rep 256

%if i=8 || i=10 || i=11 || i=12 || i=13 || i=14 || i=17

    ISR_ERR i

%else

    ISR_NOERR i

%endif


%assign i i+1

%endrep



section .data

align 8

global isr_stub_table


isr_stub_table:

%assign i 0

%rep 256

    dq isr_stub_%+i

%assign i i+1

%endrep
