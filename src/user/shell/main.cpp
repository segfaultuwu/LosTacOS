#include "LTOS/console.hpp"
#include "LTOS/drivers/keyboard.hpp"
#include "LTOS/fs/vfs.hpp"
#include "LTOS/lib/kprintf.h"
#include "LTOS/mm/pmm.hpp"
#include "LTOS/timer.hpp"

#include "multiboot.h"
#include "string.h"

#include <stdarg.h>
#include <stdint.h>

#define MAX_ARGS 32
#define MAX_TOKENS 128
#define MAX_PIPELINE 8
#define IO_BUF_SIZE 8192
#define EXPANDED_BUF_SIZE 1024

static uint64_t shell_mbi;
static multiboot_module *shell_modules;
static int shell_module_count;

static bool shell_running = false;

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

struct ShellIO {
  const char *in_data = nullptr;
  size_t in_len = 0;

  bool capture = false;
  char out_buf[IO_BUF_SIZE];
  size_t out_len = 0;
};

static void io_reset_out(ShellIO &io) {
  io.out_len = 0;
  io.out_buf[0] = '\0';
}

static void io_write_raw(ShellIO &io, const char *data, size_t len) {
  if (!io.capture) {
    console::write(data, sizeof(data));
    return;
  }

  if (len == 0)
    return;

  size_t space = IO_BUF_SIZE - 1 - io.out_len;
  if (len > space)
    len = space;

  memcpy(io.out_buf + io.out_len, data, len);
  io.out_len += len;
  io.out_buf[io.out_len] = '\0';
}

static void io_write(ShellIO &io, const char *str) {
  io_write_raw(io, str, strlen(str));
}

static void io_printf(ShellIO &io, const char *fmt, ...) {
  char tmp[512];

  va_list ap;
  va_start(ap, fmt);
  int n = kvsnprintf(tmp, sizeof(tmp), fmt, ap);
  va_end(ap);

  if (n < 0)
    return;

  if ((size_t)n >= sizeof(tmp))
    n = sizeof(tmp) - 1;

  io_write_raw(io, tmp, (size_t)n);
}

static void cmd_fetch(int, char **, ShellIO &io) { io_write(io, ASCII); }

static void cmd_help(int, char **, ShellIO &io) {
  io_write(io, "LosShellos help:\n"
               "\tfetch       - display OS info\n"
               "\thelp        - help\n"
               "\texit        - exit shell\n"
               "\tls          - list directory\n"
               "\tpwd         - current path\n"
               "\tcd          - change directory\n"
               "\tmkdir       - create directory\n"
               "\ttouch       - create file\n"
               "\trm          - remove file\n"
               "\tcat         - print file (or stdin if no arg)\n"
               "\tmemory      - memory map\n"
               "\tlsmod       - list modules\n"
               "\tclear       - clear screen\n"
               "\tuptime      - uptime\n"
               "\techo        - print text\n"
               "\nPipes and redirection are supported:\n"
               "\tcmd1 | cmd2       - pipe cmd1's output into cmd2\n"
               "\tcmd > file        - write output to file (truncate)\n"
               "\tcmd >> file       - append output to file\n"
               "\tcmd < file        - read file as input\n");
}

static void cmd_ls(int argc, char **argv, ShellIO &io) {
  fs::vfs::Node *dir;

  if (argc > 1)
    dir = fs::vfs::find(argv[1]);
  else
    dir = fs::vfs::current_dir;

  if (!dir) {
    io_printf(io, "ls: %s: no such file or directory\n", argv[1]);
    return;
  }

  if (dir->directory)
    fs::vfs::list_dir(dir);
  else
    io_printf(io, "%s\n", argv[1]);
}

static void cmd_pwd(int, char **, ShellIO &io) {
  io_printf(io, "%s\n", fs::vfs::get_path(fs::vfs::current_dir));
}

static void cmd_clear(int, char **, ShellIO &) { console::clear(); }

static void cmd_echo(int argc, char **argv, ShellIO &io) {
  for (int i = 1; i < argc; i++) {
    io_write(io, argv[i]);

    if (i + 1 < argc)
      io_write(io, " ");
  }

  io_write(io, "\n");
}

static void cmd_touch(int argc, char **argv, ShellIO &io) {
  if (argc < 2) {
    io_write(io, "touch: missing operand\n");
    return;
  }

  fs::vfs::create_file(argv[1]);
}

static void cmd_mkdir(int argc, char **argv, ShellIO &io) {
  if (argc < 2) {
    io_write(io, "mkdir: missing operand\n");
    return;
  }

  fs::vfs::create_dir(argv[1]);
}

static void cmd_cd(int argc, char **argv, ShellIO &io) {
  if (argc < 2) {
    io_write(io, "cd: missing operand\n");
    return;
  }

  fs::vfs::change_dir(argv[1]);
}

static void cmd_cat(int argc, char **argv, ShellIO &io) {
  if (argc < 2) {
    if (io.in_data) {
      io_write_raw(io, io.in_data, io.in_len);
    } else {
      io_write(io, "cat: missing operand\n");
    }
    return;
  }

  auto node = fs::vfs::find(argv[1]);

  if (!node) {
    io_write(io, "cat: file not found\n");
    return;
  }

  if (node->directory) {
    io_write(io, "cat: node is not a file\n");
    return;
  }

  char *content = fs::vfs::get_content(node);
  io_printf(io, "%s\n", content);
}

static void cmd_rm(int argc, char **argv, ShellIO &io) {
  if (argc < 2) {
    io_write(io, "rm: missing operand\n");
    return;
  }

  fs::vfs::remove(argv[1]);
}

static void cmd_memory(int, char **, ShellIO &) { pmm::print_memory_map(); }

static void cmd_uptime(int, char **, ShellIO &io) {
  uint64_t uptime_ms = timer::get_uptime_ms();

  uint64_t sec = uptime_ms / 1000;
  uint64_t ms = uptime_ms % 1000;

  uint64_t hours = sec / 3600;
  uint64_t minutes = (sec % 3600) / 60;
  uint64_t seconds = sec % 60;

  if (hours)
    io_printf(io, "%lu hours, ", hours);

  if (minutes || hours)
    io_printf(io, "%lu minutes, ", minutes);

  io_printf(io, "%lu.%03lu seconds\n", seconds, ms);
}

static void cmd_modules(int, char **, ShellIO &) {
  multiboot2::list_modules(shell_mbi, shell_modules, shell_module_count);
}

static void cmd_exit(int, char **, ShellIO &) { shell_running = false; }

struct Command {
  const char *name;
  void (*handler)(int argc, char **argv, ShellIO &io);
};

static Command commands[] = {
    {"fetch", cmd_fetch},   {"help", cmd_help},     {"ls", cmd_ls},
    {"pwd", cmd_pwd},       {"cd", cmd_cd},         {"mkdir", cmd_mkdir},
    {"touch", cmd_touch},   {"cat", cmd_cat},       {"rm", cmd_rm},
    {"memory", cmd_memory}, {"uptime", cmd_uptime}, {"lsmod", cmd_modules},
    {"clear", cmd_clear},   {"echo", cmd_echo},     {"exit", cmd_exit},
};

static void expand_operators(const char *input, char *out, size_t out_size) {
  size_t oi = 0;

  auto put = [&](char c) {
    if (oi + 1 < out_size)
      out[oi++] = c;
  };

  auto put_str = [&](const char *s) {
    while (*s)
      put(*s++);
  };

  for (const char *p = input; *p;) {
    if (*p == '>' && *(p + 1) == '>') {
      put_str(" >> ");
      p += 2;
    } else if (*p == '>') {
      put_str(" > ");
      p += 1;
    } else if (*p == '<') {
      put_str(" < ");
      p += 1;
    } else if (*p == '|') {
      put_str(" | ");
      p += 1;
    } else {
      put(*p);
      p += 1;
    }
  }

  out[oi < out_size ? oi : out_size - 1] = '\0';
}

struct Segment {
  char *argv[MAX_ARGS];
  int argc = 0;

  const char *in_file = nullptr;
  const char *out_file = nullptr;
  bool append = false;
};

static int build_pipeline(char **tokens, int ntok, Segment *segs,
                          int max_segs) {
  int nseg = 0;
  segs[0] = Segment{};

  for (int i = 0; i < ntok; i++) {
    char *tok = tokens[i];

    if (strcmp(tok, "|") == 0) {
      nseg++;
      if (nseg >= max_segs)
        return -1;
      segs[nseg] = Segment{};
      continue;
    }

    if (strcmp(tok, ">") == 0 || strcmp(tok, ">>") == 0) {
      bool append = tok[1] == '>';
      if (i + 1 >= ntok)
        return -1;

      segs[nseg].out_file = tokens[++i];
      segs[nseg].append = append;
      continue;
    }

    if (strcmp(tok, "<") == 0) {
      if (i + 1 >= ntok)
        return -1;

      segs[nseg].in_file = tokens[++i];
      continue;
    }

    if (segs[nseg].argc < MAX_ARGS - 1)
      segs[nseg].argv[segs[nseg].argc++] = tok;
  }

  return nseg + 1;
}

static void run_pipeline(char *input) {
  char expanded[EXPANDED_BUF_SIZE];
  expand_operators(input, expanded, sizeof(expanded));

  char *tokens[MAX_TOKENS];
  int ntok = strsplt(expanded, tokens, MAX_TOKENS);

  if (ntok == 0)
    return;

  Segment segs[MAX_PIPELINE];
  int nseg = build_pipeline(tokens, ntok, segs, MAX_PIPELINE);

  if (nseg <= 0) {
    kprintf("shell: syntax error\n");
    return;
  }

  static ShellIO stage_io[MAX_PIPELINE];

  const char *carry_data = nullptr;
  size_t carry_len = 0;

  for (int i = 0; i < nseg; i++) {
    Segment &seg = segs[i];

    if (seg.argc == 0) {
      kprintf("shell: syntax error: empty command in pipeline\n");
      return;
    }

    ShellIO &io = stage_io[i];
    io_reset_out(io);

    if (seg.in_file) {
      auto node = fs::vfs::find(seg.in_file);
      if (!node || node->directory) {
        kprintf("shell: %s: no such file\n", seg.in_file);
        return;
      }
      char *content = fs::vfs::get_content(node);
      io.in_data = content;
      io.in_len = content ? strlen(content) : 0;
    } else if (carry_data) {
      io.in_data = carry_data;
      io.in_len = carry_len;
    } else {
      io.in_data = nullptr;
      io.in_len = 0;
    }

    bool last_stage = (i == nseg - 1);
    io.capture = !last_stage || (seg.out_file != nullptr);

    bool found = false;
    for (auto &cmd : commands) {
      if (strcmp(seg.argv[0], cmd.name) == 0) {
        cmd.handler(seg.argc, seg.argv, io);
        found = true;
        break;
      }
    }

    if (!found) {
      kprintf("Unknown command: '%s'\n", seg.argv[0]);
      return;
    }

    if (seg.out_file) {
      if (seg.append)
        fs::vfs::append_content(seg.out_file, io.out_buf, io.out_len);
      else
        fs::vfs::write_content(seg.out_file, io.out_buf, io.out_len);
    }

    if (!last_stage) {
      carry_data = io.out_buf;
      carry_len = io.out_len;
    }
  }
}

int shell_main(uint64_t mbi_phys_addr, multiboot_module *mb_out,
               int mb_max_count) {
  shell_running = true;

  shell_mbi = mbi_phys_addr;
  shell_modules = mb_out;
  shell_module_count = mb_max_count;

  while (shell_running) {
    kprintf("\033[32mroot@lostacos\033[0m:"
            "\033[34m%s\033[0m"
            "\033[31m#\033[0m ",
            fs::vfs::get_path(fs::vfs::current_dir));

    char *input = drivers::keyboard::getstring();

    if (input[0] == '\0')
      continue;
    kprintf("\n");

    run_pipeline(input);
  }

  return 0;
}
