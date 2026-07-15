#include "LTOS/drivers/serial.hpp"
#include "LTOS/lib/kprintf.h"
#include "LTOS/panic.hpp"
#include "LTOS/timer.hpp"
#include "LTOS/vga.hpp"

extern int shell_main(void);

extern "C" void kernel_main() {
  drivers::serial::init();
  drivers::serial::write("Reached kernel_main!\n");

  timer::init(100);

  vga::clear();

  kprintf("LosTacOS booted\n");

  drivers::serial::write("About to run shell..\n");
  int err = shell_main();
  kprintf("Shell exited with code %d", err);
  panic::halt("Should not exit the main loop.");
}
