#include "LTOS/arch/x86_64/gdt.hpp"
#include "LTOS/arch/x86_64/idt.hpp"
#include "LTOS/arch/x86_64/paging.hpp"
#include "LTOS/drivers/pic.hpp"
#include "LTOS/drivers/serial.hpp"
#include "LTOS/lib/kprintf.h"
#include "LTOS/logger.hpp"
#include "LTOS/panic.hpp"
#include "LTOS/timer.hpp"
#include "LTOS/user/userspace.hpp"
#include "LTOS/vga.hpp"

extern int shell_main(void);

extern "C" void kernel_main() {
  drivers::serial::init();
  drivers::serial::write("Reached kernel_main!\n");

  vga::clear();

  drivers::pic::init();
  timer::init(100);

  gdt::init();
  idt::init();

  paging::init();
  paging::setup_kernel_identity();
  paging::enable_paging();

  asm volatile("sti");

  logger::info("LosTacOS booted\n");
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

  user::run_user();

  drivers::serial::write("About to run shell..\n");
  int err = shell_main();
  kprintf("Shell exited with code %d", err);
  panic::halt("Should not exit the main loop.");
}
