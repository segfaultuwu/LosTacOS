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
