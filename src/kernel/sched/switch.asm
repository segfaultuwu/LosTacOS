global switch_context
global exec_enter


section .text


; void exec_enter(Registers *r);
;
; Drops straight into a freshly-built Registers frame (same layout create_task
; uses: r15..rax, vector, error, rip, cs, rflags, rsp, ss) and never returns to
; the caller -- used by exec() to hand control to the newly loaded program
; instead of unwinding back through the syscall's own iretq, which would
; otherwise resume the old (already-destroyed) program image.
exec_enter:

    mov rsp, rdi

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

    add rsp, 16 ; skip vector, error

    iretq


switch_context:


    mov [rdi], rsp


    pop rax
    pop rbx
    pop rcx
    pop rdx

    pop rsi
    pop rdi

    pop rbp

    pop r8
    pop r9
    pop r10
    pop r11

    pop r12
    pop r13
    pop r14
    pop r15


    ret
