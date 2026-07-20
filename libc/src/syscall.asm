global syscall

section .text

syscall:

    mov r8, rcx        ; zachowaj c

    mov rax, rdi       ; syscall number
    mov rbx, rsi       ; arg1
    mov rcx, rdx       ; arg2
    mov rdx, r8        ; arg3

    int 0x80

    ret
