#include "LTOS/arch/x86_64/paging.hpp"
#include "LTOS/boot.hpp"
#include "LTOS/drivers/serial.hpp"
#include "LTOS/lib/kprintf.h"
#include "LTOS/panic.hpp"
#include "LTOS_gen/version.h"
#include "multiboot.h"

extern int shell_main(uint64_t mbi_phys_addr, struct multiboot_module *mb_out,
                      int mb_max_count);

extern "C" void kernel_main(uint64_t magic, uint64_t mbi_addr) {
  if (magic == 0x36d76289) {
    uint32_t mbi_total_size = *(uint32_t *)mbi_addr;
    paging::reserve_below(mbi_addr + mbi_total_size);
  }

  boot::setup();

  kprintf("magic=%x mbi=%x\n", magic, mbi_addr);

  struct multiboot_module mods[8];

  if (magic == 0x36d76289) {
    multiboot2::parse_info(mbi_addr);
  } else {
    kprintf("Invalid multiboot2 magic!\n");
  }

  kprintf("LosTacOS v%s booted\n", LTOS_VERSION);
  /* Idt testing shit
   *
   * logger::info("Trying to divide by 0..");
   * volatile int a = 1;
   * volatile int b = 0;
   * volatile int tmp = a / b;
   * kprintf("result: %d", tmp);
   *
   * Timer testing shit
   *
   * while (true) {
   *   if (timer::ticks() % 100 == 0)
   *     logger::info("1 second");
   *
   *   asm volatile("hlt");
   * }

    while (true) {
      logger::info("test");
      timer::sleep(1000);
    }
   */

  drivers::serial::write("About to run shell..\n");
  int err = shell_main(mbi_addr, mods, 8);
  kprintf("Shell exited with code %d", err);
  panic::halt("Should not exit the main loop.");
}
