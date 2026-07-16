global lidt

section .text

lidt:
    lidt [rdi]
    ret

global isr0
extern divide_error

isr0:
    cli

    push rax
    push rbx
    push rcx
    push rdx

    call divide_error

    pop rdx
    pop rcx
    pop rbx
    pop rax

    iretq

global irq0
extern timer_irq

irq0:
    push rax

    call timer_irq

    pop rax

    iretq

extern unhandled_interrupt

isr_stub_common:
    mov rdi, [rsp]
    call unhandled_interrupt
.hang:
    hlt
    jmp .hang

%macro ISR_STUB 1
isr_stub_%1:
    cli
    push qword %1
    jmp isr_stub_common
%endmacro

%assign i 0
%rep 256
    ISR_STUB i
%assign i i+1
%endrep

section .data
global isr_stub_table
isr_stub_table:
%assign i 0
%rep 256
    dq isr_stub_%+i
%assign i i+1
%endrep
