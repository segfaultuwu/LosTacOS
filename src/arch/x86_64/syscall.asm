BITS 64

section .text

global isr128
extern syscall_handler
extern set_current_regs


isr128:

    cli

    push 0
    push 128

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
    call set_current_regs

    ; userspace (libc/src/syscall.asm) hands us: rax=num rdi=a1 rsi=a2 rdx=a3
    ; r10=a4 r8=a5 r9=a6. After the pushes above, those live at:
    ;   rax=112 rdi=72 rsi=80 rdx=88 r10=40 r8=56 r9=48

    mov rax,[rsp+48]  ; arg6 (was r9) -- grab before the stack shifts below
    push rax          ; pass it as syscall_handler's 7th (stack) argument

    mov rdi,[rsp+8+112] ; syscall number
    mov rsi,[rsp+8+72]  ; arg1
    mov rdx,[rsp+8+80]  ; arg2
    mov rcx,[rsp+8+88]  ; arg3
    mov r8, [rsp+8+40]  ; arg4 (was r10)
    mov r9, [rsp+8+56]  ; arg5 (was r8)


    call syscall_handler

    add rsp, 8        ; drop the arg6 slot we pushed


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

    add rsp, 16       ; drop vector and error

    iretq
