BITS 32

DEFAULT REL

GLOBAL _start
GLOBAL mb_magic
GLOBAL mb_info

EXTERN kernel_main


; ============================================================
; MULTIBOOT2 HEADER (GRUB)
; ============================================================

SECTION .multiboot

align 8

mb_header:

    dd 0xE85250D6
    dd 0
    dd mb_header_end - mb_header
    dd -(0xE85250D6 + (mb_header_end - mb_header))


; framebuffer request

align 8

mb_fb_tag:

    dw 5
    dw 0
    dd 20

    dd 1024
    dd 768
    dd 32


; end tag

align 8

mb_end:

    dw 0
    dw 0
    dd 8


mb_header_end:



; ============================================================
; LIMINE REQUESTS
; ============================================================

SECTION .limine_requests

align 8


limine_base_revision:

    dq 0xC7B1DD30DF4C8B88
    dq 0x0A82E883A194F07B
    dq 0



align 8


limine_framebuffer_request:

    dq 0xC7B1DD30DF4C8B88
    dq 0x0A82E883A194F07B

    dq 0

    dq 1024
    dq 768
    dq 32



align 8


limine_requests_end:

    dq 0
    dq 0



; ============================================================
; ENTRY
; ============================================================

SECTION .text


_start:

    cli
    cld


    ; save GRUB arguments
    mov [mb_magic], eax
    mov [mb_info], ebx



    mov esp, stack_top



    call check_cpuid
    call check_long_mode
    call setup_page_tables



    ; enable PAE

    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax



    ; load PML4

    mov eax, pml4_table
    mov cr3, eax



    ; enable long mode

    mov ecx,0xC0000080

    rdmsr

    or eax,1<<8

    wrmsr



    ; enable paging

    mov eax,cr0

    or eax,1<<31

    mov cr0,eax



    lgdt [gdt64_pointer]


    jmp CODE_SEL:long_mode





; ============================================================
; LONG MODE
; ============================================================

BITS 64


long_mode:


    mov ax,DATA_SEL

    mov ds,ax
    mov es,ax
    mov ss,ax



    mov rsp,stack_top



    ;
    ; Detect bootloader
    ;


    cmp dword [mb_magic],0x36D76289

    je boot_grub



boot_limine:


    mov rdi,0x4C494D49


    lea rsi,[rel limine_framebuffer_request]


    jmp call_kernel




boot_grub:


    mov rdi,0x36D76289


    mov esi,[mb_info]


call_kernel:


    and rsp,-16

    sub rsp,8


    call kernel_main



hang:

    cli

    hlt

    jmp hang





; ============================================================
; CPU CHECK
; ============================================================


BITS 32


check_cpuid:


    pushfd

    pop eax


    mov ecx,eax


    xor eax,0x200000


    push eax

    popfd


    pushfd

    pop eax


    xor eax,ecx


    jz cpuid_fail


    ret



cpuid_fail:


    cli

    hlt

    jmp cpuid_fail





check_long_mode:


    mov eax,0x80000000

    cpuid


    cmp eax,0x80000001

    jb longmode_fail



    mov eax,0x80000001

    cpuid


    test edx,1<<29


    jz longmode_fail


    ret




longmode_fail:


    cli

    hlt

    jmp longmode_fail





; ============================================================
; PAGE TABLES
; ============================================================


setup_page_tables:


    mov eax,pdp_table

    or eax,3

    mov [pml4_table],eax



    mov eax,pd_table0

    or eax,3

    mov [pdp_table],eax



    mov eax,pd_table1

    or eax,3

    mov [pdp_table+8],eax



    mov eax,pd_table2

    or eax,3

    mov [pdp_table+16],eax



    mov eax,pd_table3

    or eax,3

    mov [pdp_table+24],eax



    mov edi,pd_table0


    mov eax,0x83


    mov ecx,2048



paging_map_loop:


    mov [edi],eax


    add eax,0x200000


    add edi,8


    dec ecx

    jnz paging_map_loop


    ret





; ============================================================
; GDT
; ============================================================


SECTION .rodata


align 8


gdt64:

    dq 0


gdt_code:

    dq 0x00209A0000000000


gdt_data:

    dq 0x0000920000000000



gdt64_pointer:

    dw gdt64_pointer-gdt64-1

    dq gdt64



CODE_SEL equ gdt_code-gdt64
DATA_SEL equ gdt_data-gdt64





; ============================================================
; BSS
; ============================================================


SECTION .bss


align 4096


pml4_table:

    resb 4096


pdp_table:

    resb 4096


pd_table0:

    resb 4096


pd_table1:

    resb 4096


pd_table2:

    resb 4096


pd_table3:

    resb 4096



align 4


mb_magic:

    resd 1


mb_info:

    resd 1




align 16


stack_bottom:

    resb 16384


stack_top:
