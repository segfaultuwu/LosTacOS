#include "LTOS/drivers/keyboard.hpp"
#include "LTOS/fs/vfs.hpp"
#include "LTOS/lib/kprintf.h"
#include "LTOS/mm/pmm.hpp"
#include "LTOS/timer.hpp"
#include "LTOS/vga.hpp"

#include "multiboot.h"
#include "string.h"

#include <stdint.h>

static uint64_t shell_mbi;
static multiboot_module *shell_modules;
static int shell_module_count;

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

static void cmd_fetch(char *) { vga::write(ASCII); }

static void cmd_help(char *) {
  vga::write("LosShellos help:\n"
             "\tfetch       - display OS info\n"
             "\thelp        - this help\n"
             "\texit        - exit shell\n"
             "\tls          - list directory\n"
             "\tpwd         - show current path\n"
             "\tmemory      - memory map\n"
             "\tuptime      - uptime\n");
}

static void cmd_ls(char *) { fs::vfs::list_dir(); }

static void cmd_pwd(char *) {
  kprintf("%s\n", fs::vfs::get_path(fs::vfs::current_dir));
}

static void cmd_memory(char *) { pmm::print_memory_map(); }

static void cmd_uptime(char *) {
  uint64_t uptime_ms = timer::get_uptime_ms();

  uint64_t sec = uptime_ms / 1000;
  uint64_t ms = uptime_ms % 1000;

  uint64_t hours = sec / 3600;
  uint64_t minutes = (sec % 3600) / 60;
  uint64_t seconds = sec % 60;

  if (hours)
    kprintf("%lu hours, ", hours);

  if (minutes || hours)
    kprintf("%lu minutes, ", minutes);

  kprintf("%lu.%03lu seconds\n", seconds, ms);
}

static void cmd_modules(char *) {
  multiboot2::list_modules(shell_mbi, shell_modules, shell_module_count);
}

static bool shell_running = true;

static void cmd_exit(char *) { shell_running = false; }

struct Command {
  const char *name;
  void (*handler)(char *);
};

static Command commands[] = {
    {"fetch", cmd_fetch},
    {"help", cmd_help},
    {"ls", cmd_ls},
    {"pwd", cmd_pwd},
    {"memory", cmd_memory},
    {"uptime", cmd_uptime},
    {"listmodules", cmd_modules},
    {"exit", cmd_exit},
};

static void execute_command(char *input) {
  for (auto &cmd : commands) {
    if (strcmp(input, cmd.name) == 0) {
      cmd.handler(nullptr);
      return;
    }
  }

  kprintf("Unknown command: '%s'\n", input);
}

int shell_main(uint64_t mbi_phys_addr, multiboot_module *mb_out,
               int mb_max_count) {
  shell_mbi = mbi_phys_addr;
  shell_modules = mb_out;
  shell_module_count = mb_max_count;

  vga::write("Welcome to LosShellos!\n");

  char *input;

  while (shell_running) {
    vga::write("root");
    vga::write("\033[32m@\033[0m");
    vga::write("lostacos");
    vga::write("\033[32m#\033[0m ");

    vga::write(fs::vfs::get_path(fs::vfs::current_dir));

    vga::write(" > ");

    input = drivers::keyboard::getstring();

    if (input[0] == '\0')
      continue;

    execute_command(input);

    vga::write("\n");
  }

  return 0;
}
