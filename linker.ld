ENTRY(_start)

SECTIONS
{
  . = 1M;

  _kernel_start = .;

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

  _kernel_end = .;
}
