#include "LTOS/drivers/keyboard.hpp"
#include "LTOS/lib/kprintf.h"
#include "LTOS/vga.hpp"
#include "multiboot.h"
#include "string.h"
#include <cstdint>
#include <cstdio>

const static char *ASCII = "\033[31m"
                           " ___      _______  _______  _______  _______  "
                           "_______  _______  _______  \n"

                           "\033[33m"
                           "|   |    |       ||       ||       ||   _   ||     "
                           "  ||       ||       | \n"

                           "\033[32m"
                           "|   |    |   _   ||  _____||_     _||  |_|  ||     "
                           "  ||   _   ||  _____| \n"

                           "\033[34m"
                           "|   |    |  | |  || |_____   |   |  |       ||     "
                           "  ||  | |  || |_____  \n"

                           "\033[35m"
                           "|   |___ |  |_|  ||_____  |  |   |  |       ||     "
                           " _||  |_|  ||_____  | \n"

                           "\033[36m"
                           "|       ||       | _____| |  |   |  |   _   ||     "
                           "|_ |       | _____| | \n"

                           "\033[37m"
                           "|_______||_______||_______|  |___|  |__| "
                           "|__||_______||_______||_______| \n"

                           "\033[0m";

const static char *LOS_SHELLOS_HELP_TEXT =
    "LosShellos help:\n"
    "\tfetch       - display info about the OS\n"
    "\texit        - exits the shell\n"
    "\tlistmodules - lists multiboot modules\n";

int shell_main(uint64_t mbi_phys_addr, struct multiboot_module *mb_out,
               int mb_max_count) {
  char *input;

  vga::write("Welcome to LosShellos!\n");
  while (true) {
    vga::write("LosShellos> ");
    input = drivers::keyboard::getstring();
    if (strcmp(input, "fetch") == 0) {
      vga::write(ASCII);
    } else if (strcmp(input, "help") == 0) {
      vga::write(LOS_SHELLOS_HELP_TEXT);
    } else if (strcmp(input, "listmodules") == 0) {
      multiboot2::list_modules(mbi_phys_addr, mb_out, mb_max_count);
    } else if (strcmp(input, "exit") == 0) {
      return 0;
      // TODO: fix bug where empty string still shows unknown command
    } else if (input[0] == '\0' || input[0] == '\n')
      break;
    else
      kprintf("Unknown command: '%s'", input);
    vga::write("\n");
  }
}
