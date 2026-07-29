global gdt_flush
global tss_flush

gdt_flush:
    lgdt [rdi]
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    ret

; void tss_flush(uint16_t selector);
tss_flush:
    mov ax, di
    ltr ax
    ret
