global syscall

section .text

syscall:

    ; in (System V AMD64 C ABI -- how this function itself is called):
    ; rdi num
    ; rsi a1
    ; rdx a2
    ; rcx a3
    ; r8  a4
    ; r9  a5
    ; [rsp+8] a6   (7th arg, pushed by our caller before `call`)
    ;
    ; out (what the kernel's isr128 / syscall_handler expects, see
    ; src/arch/x86_64/syscall.asm):
    ; rax num
    ; rdi a1
    ; rsi a2
    ; rdx a3
    ; r10 a4
    ; r8  a5
    ; r9  a6

    mov r10, r8       ; a4 -> r10
    mov r8,  r9       ; a5 -> r8
    mov r9,  [rsp+8]  ; a6 -> r9 (grab it off the stack before it's gone)

    mov rax, rdi      ; num -> rax
    mov rdi, rsi      ; a1  -> rdi
    mov rsi, rdx      ; a2  -> rsi
    mov rdx, rcx      ; a3  -> rdx

    int 0x80

    ret
