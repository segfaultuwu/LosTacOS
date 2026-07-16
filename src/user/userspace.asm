global enter_userspace

enter_userspace:
    cli

    mov ax, 0x23
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push 0x23
    push rsi

    pushfq

    push 0x1B
    push rdi

    iretq
