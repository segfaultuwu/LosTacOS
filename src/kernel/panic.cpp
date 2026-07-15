#include "LTOS/panic.hpp"
#include "LTOS/vga.hpp"
namespace panic {

void halt(const char *msg) {
  vga::write("\n\033[31mPANIC:\033[0m ");
  vga::write(msg);

  while (true)
    asm volatile("cli; hlt");
}

} // namespace panic
