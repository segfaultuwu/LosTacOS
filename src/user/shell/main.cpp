#include "LTOS/drivers/keyboard.hpp"
#include "LTOS/vga.hpp"
#include "string.h"

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
    "\tfetch - display info about the OS\n";

int shell_main() {
  char *input;

  vga::write("Welcome to LosShellos!\n");
  while (true) {
    vga::write("LosShellos> ");
    input = drivers::keyboard::getstring();
    if (strcmp(input, "fetch") == 0) {
      vga::write(ASCII);
    } else if (strcmp(input, "help") == 0) {
      vga::write(LOS_SHELLOS_HELP_TEXT);
    } else if (strcmp(input, "exit") == 0) {
      return 0;
    }
    vga::write("\n");
  }
}
