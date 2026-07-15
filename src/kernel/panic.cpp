#include "LTOS/panic.hpp"
#include "LTOS/vga.hpp"
namespace panic {

void halt(const char *msg) {
  vga::write("\nPANIC: ");
  vga::write(msg);

  while (true)
    asm volatile("cli; hlt");
}

} // namespace panic
