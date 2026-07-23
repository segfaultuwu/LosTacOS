BITS 64

section .text

extern unhandled_interrupt

; =========================================================
; Common exception handler
;
; Stack on entry:
;
; CPU pushes:
;   error (only for exceptions with error code)
;   rip
;   cs
;   rflags
;   rsp
;   ss
;
; Stub pushes:
;   vector
;
; So:
;
; rsp+0   vector
; rsp+8   error code
; rsp+16  rip
; rsp+24  cs
; rsp+32  rflags
; rsp+40  rsp
; rsp+48  ss
;
; =========================================================

isr_common:

    cli


    mov rdi, [rsp+0]       ; vector
    mov rsi, [rsp+8]       ; error code


    xor rdx, rdx           ; addr = 0


    cmp rdi, 14            ; #PF
    jne .get_rip


    mov rdx, cr2           ; page fault address


.get_rip:

    mov rcx, [rsp+16]      ; instruction pointer


    call unhandled_interrupt


.hang:

    hlt
    jmp .hang



; =========================================================
; Exception macros
; =========================================================


%macro ISR_ERR 1

global isr_stub_%1

isr_stub_%1:

    cli

    ; CPU already pushed error code
    push qword %1

    jmp isr_common

%endmacro



%macro ISR_NOERR 1

global isr_stub_%1

isr_stub_%1:

    cli

    ; create fake error code
    push qword 0

    push qword %1

    jmp isr_common

%endmacro



; =========================================================
; Generate all 256 vectors
; =========================================================


%assign i 0

%rep 256

%if i=8 || i=10 || i=11 || i=12 || i=13 || i=14 || i=17

    ISR_ERR i

%else

    ISR_NOERR i

%endif

%assign i i+1

%endrep



; =========================================================
; IDT table
; =========================================================

section .data

align 8

global isr_stub_table


isr_stub_table:


%assign i 0

%rep 256

    dq isr_stub_%+i

%assign i i+1

%endrep
