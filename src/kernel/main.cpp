#include "LTOS/drivers/keyboard.hpp"
#include "LTOS/panic.hpp"
#include "LTOS/vga.hpp"

extern "C" void kernel_main() {
  vga::clear();

  vga::write("LosTacOS booted\n");
  vga::write("Entered x86_64 long mode\n");
  char c;
  char *s;

  vga::write("Enter a character:");
  c = drivers::keyboard::getchar();
  vga::put(c);
  vga::put('\n');

  vga::write("Enter a string:");
  s = drivers::keyboard::getstring();
  vga::write(s);
  vga::put('\n');

  panic::halt("Should not exit the main loop.");
}
