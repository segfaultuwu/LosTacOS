#include "LTOS/panic.hpp"
#include "LTOS/drivers/console.hpp"
#include <string.h>

namespace panic {

void halt(const char *msg) {
  const char *panic = "\n\033[31mPANIC:\033[0m ";
  console::write(panic, strlen(panic));
  console::write(msg, strlen(msg));

  while (true)
    asm volatile("cli; hlt");
}

} // namespace panic
