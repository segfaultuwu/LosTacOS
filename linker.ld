ENTRY(_start)

SECTIONS
{
  . = 1M;

  .text : {
    *(.multiboot)
    *(.text*)
  }

  .rodata : {
      *(.rodata*)
  }

  .data : {
    *(.data*)
  }

  .bss : ALIGN(4096) {
    *(COMMON)
    *(.bss*)
  }

  kernel_end = .;
}
