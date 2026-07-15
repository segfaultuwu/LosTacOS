#include "LTOS/arch/x86_64/idt.hpp"
#include "LTOS/drivers/pic.hpp"
#include "LTOS/drivers/serial.hpp"
#include "LTOS/lib/kprintf.h"
#include "LTOS/logger.hpp"
#include "LTOS/panic.hpp"
#include "LTOS/timer.hpp"
#include "LTOS/vga.hpp"

extern int shell_main(void);

extern "C" void kernel_main() {
  drivers::serial::init();
  drivers::serial::write("Reached kernel_main!\n");
  idt::init();
  drivers::pic::init();
  timer::init(100);
  asm volatile("sti");
  vga::clear();

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

  drivers::serial::write("About to run shell..\n");
  int err = shell_main();
  kprintf("Shell exited with code %d", err);
  panic::halt("Should not exit the main loop.");
}
