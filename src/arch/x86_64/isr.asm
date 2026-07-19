; Thank you claude <3

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

    global irq1_handler

    extern keyboard_irq


irq1_handler:

    push rax
    push rbx
    push rcx
    push rdx

    call keyboard_irq

    pop rdx
    pop rcx
    pop rbx
    pop rax


    mov al, 0x20
    out 0x20, al


    iretq

extern unhandled_interrupt

; Stack layout when isr_stub_common runs, for EVERY vector (thanks to the
; dummy error-code push added below for vectors that don't get one from the
; CPU):
;   [rsp+0]  = vector       (pushed by our stub)
;   [rsp+8]  = error_code   (pushed by CPU, or 0 dummy pushed by our stub)
;   [rsp+16] = rip          (pushed by CPU)
;   [rsp+24] = cs
;   [rsp+32] = rflags
;   [rsp+40] = rsp (user)
;   [rsp+48] = ss
isr_stub_common:
    cli

    mov rdi, [rsp]        ; arg1 = vector
    mov rsi, [rsp+8]      ; arg2 = error code (real or dummy 0)
    mov rdx, [rsp+16]     ; arg3 = actual faulting rip

    cmp rdi, 14
    jne .call

    mov rax, cr2
    mov rsi, rax          ; for #PF specifically, arg2 = fault address instead

.call:
    call unhandled_interrupt

.hang:
    hlt
    jmp .hang

; Vectors that push a hardware error code (Intel SDM Vol 3, 6.15):
; #DF(8) #TS(10) #NP(11) #SS(12) #GP(13) #PF(14) #AC(17)
%macro ISR_ERR 1
isr_stub_%1:
    cli
    push qword %1
    jmp isr_stub_common
%endmacro

; All other vectors: no hardware error code, so push a dummy 0 to keep the
; stack layout identical to the ISR_ERR case.
%macro ISR_NOERR 1
isr_stub_%1:
    cli
    push qword 0
    push qword %1
    jmp isr_stub_common
%endmacro

%assign i 0
%rep 256
    %if (i = 8) || (i = 10) || (i = 11) || (i = 12) || (i = 13) || (i = 14) || (i = 17)
        ISR_ERR i
    %else
        ISR_NOERR i
    %endif
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
