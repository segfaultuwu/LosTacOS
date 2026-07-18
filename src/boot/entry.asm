BITS 32

GLOBAL _start
EXTERN kernel_main


SECTION .multiboot
align 8

header:
    dd 0xE85250D6
    dd 0
    dd header_end - header
    dd -(0xE85250D6 + (header_end - header))


; framebuffer request
align 8
framebuffer_request:
    dw 5
    dw 0
    dd 20

    dd 1024
    dd 768
    dd 32


; end tag (required)
align 8
end_tag:
    dw 0
    dw 0
    dd 8


header_end:


SECTION .text

_start:
    cli
    cld

    ; Multiboot2:
    ; eax = magic
    ; ebx = mbi address

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
    mov ecx, 0xC0000080
    rdmsr

    or eax, 1 << 8

    wrmsr


    ; enable paging
    mov eax, cr0
    or eax, 1 << 31

    mov cr0, eax


    lgdt [gdt64_pointer]

    jmp CODE_SEL:long_mode_start

    align 8

BITS 64

long_mode_start:

    mov ax, DATA_SEL

    mov ds, ax
    mov es, ax
    mov ss, ax


    mov rsp, stack_top

    ; SysV ABI:
    ; rdi = arg1
    ; rsi = arg2

    mov edi, [mb_magic]
    mov esi, [mb_info]


    ; stack alignment
    and rsp, -16
    sub rsp, 8

    call kernel_main


.hang:
    hlt
    jmp .hang



BITS 32


check_cpuid:

    pushfd
    pop eax

    mov ecx, eax

    xor eax, 0x200000

    push eax
    popfd

    pushfd
    pop eax

    xor eax, ecx

    jz cpuid_fail

    ret



cpuid_fail:

    cli
    hlt
    jmp $



check_long_mode:

    mov eax, 0x80000000
    cpuid

    cmp eax, 0x80000001
    jb long_mode_fail


    mov eax, 0x80000001
    cpuid

    test edx, 1 << 29

    jz long_mode_fail

    ret



long_mode_fail:

    cli
    hlt
    jmp $



setup_page_tables:

    ; PML4 -> PDP
    mov eax, pdp_table
    or eax, 3
    mov [pml4_table], eax


    ; PDP entries -> 4 page directories

    mov eax, pd_table0
    or eax, 3
    mov [pdp_table], eax


    mov eax, pd_table1
    or eax, 3
    mov [pdp_table + 8], eax


    mov eax, pd_table2
    or eax, 3
    mov [pdp_table + 16], eax


    mov eax, pd_table3
    or eax, 3
    mov [pdp_table + 24], eax


    ; map 4GB
    mov edi, pd_table0
    mov eax, 0x83
    mov ecx, 2048


.map:

    mov [edi], eax

    add eax, 0x200000
    add edi, 8

    dec ecx
    jnz .map


    ret


SECTION .rodata

align 8

gdt64:

    dq 0


gdt_code:
    dq 0x00209A0000000000


gdt_data:
    dq 0x0000920000000000



gdt64_pointer:

    dw gdt64_pointer - gdt64 - 1
    dq gdt64



CODE_SEL equ gdt_code - gdt64
DATA_SEL equ gdt_data - gdt64



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
