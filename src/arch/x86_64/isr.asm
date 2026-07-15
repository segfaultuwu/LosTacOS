global lidt

section .text

lidt:
    lidt [rdi]
    ret

global isr0
extern divide_error

section .text

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
