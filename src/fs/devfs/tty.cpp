#include "LTOS/drivers/tty.hpp"

#include "LTOS/drivers/console.hpp"
#include "LTOS/drivers/serial.hpp"
#include "LTOS/drivers/tty/ioctl.hpp"
#include "LTOS/fs/devfs.hpp"
#include "LTOS/lib/kprintf.h"

#include <string.h>

extern volatile size_t stdin_len;
extern char stdin_buffer[256];

namespace tty {

static struct termios tio = {
    .c_iflag = 0,
    .c_oflag = 0,
    .c_cflag = 0,
    .c_lflag = ISIG | ICANON | ECHO,
    .c_line = 0,
    .c_cc = {},
};

static bool termios_init = false;

static void ensure_defaults() {
  if (termios_init)
    return;

  termios_init = true;

  tio.c_cc[VERASE] = '\b';
  tio.c_cc[VEOF] = 0x04;
  tio.c_cc[VINTR] = 0x03;
  tio.c_cc[VMIN] = 1;
  tio.c_cc[VTIME] = 0;
}

static char pop_raw_byte() {
  char c = stdin_buffer[0];

  for (size_t i = 1; i < stdin_len; i++)
    stdin_buffer[i - 1] = stdin_buffer[i];

  stdin_len--;

  return c;
}

static size_t read_canonical(char *buf, size_t len) {
  bool echo = tio.c_lflag & ECHO;
  char erase = (char)tio.c_cc[VERASE];

  size_t n = 0;

  while (n < len) {
    if (stdin_len == 0) {
      asm volatile("sti; hlt");
      continue;
    }

    char c = pop_raw_byte();

    if (c == 0x08 || c == 127 || (erase && c == erase)) {
      if (n > 0) {
        n--;
        if (echo) {
          console::put(0x08);
        }
      }
      continue;
    }

    buf[n++] = c;

    if (echo) {
      console::put(c);
    }

    if (c == '\n')
      break;
  }

  return n;
}

static size_t read_raw(char *buf, size_t len) {
  if (len == 0)
    return 0;

  if (stdin_len == 0 && tio.c_cc[VMIN] == 0) {
    return 0;
  }

  while (stdin_len == 0) {
    asm volatile("sti; hlt");
  }


  size_t n = 0;

  while (n < len && stdin_len > 0) {
    char c = pop_raw_byte();

    buf[n++] = c;
  }

  return n;
}

size_t read(char *buf, size_t len) {
  ensure_defaults();

  if (tio.c_lflag & ICANON)
    return read_canonical(buf, len);

  return read_raw(buf, len);
}

size_t write(const char *buf, size_t len) {
  console::write(buf, len);

  return len;
}

int ioctl(unsigned long req, void *arg) {
  ensure_defaults();

  if (!arg)
    return -1;

  switch (req) {

  case TIOCGWINSZ: {
    auto *ws = (winsize *)arg;

    ws->ws_row = (uint16_t)console::get_rows();
    ws->ws_col = (uint16_t)console::get_cols();
    ws->ws_xpixel = 0;
    ws->ws_ypixel = 0;

    return 0;
  }

  case TCGETS: {
    memcpy(arg, &tio, sizeof(termios));
    return 0;
  }

  case TCSETS:
  case TCSETSW:
  case TCSETSF: {
    memcpy(&tio, arg, sizeof(termios));
    return 0;
  }

  default:
    return -1;
  }
}

static size_t dev_read(char *buf, size_t len, size_t offset) {
  (void)offset;
  return read(buf, len);
}

static size_t dev_write(const char *buf, size_t len, size_t offset) {
  (void)offset;
  return write(buf, len);
}

static fs::vfs::DevOps tty_ops = {.write = dev_write, .read = dev_read, .ioctl = ioctl};

static int active_vt = 1;

void switch_vt(int vt) {
  if (vt < 1 || vt > 12)
    return;
  active_vt = vt;
  console::write("\033[2J\033[H", 7);
  char msg[64];
  int len = ksnprintf(msg, sizeof(msg), "[Switched to /dev/tty%d]\n", vt);
  if (len > 0) {
    console::write(msg, len);
  }
}

int get_active_vt() {
  return active_vt;
}

void init() {
  ensure_defaults();
  fs::devfs::register_device("tty", &tty_ops);
  fs::devfs::register_device("ptmx", &tty_ops);
  fs::devfs::register_device("console", &tty_ops);
  fs::devfs::register_device("stdin", &tty_ops);
  fs::devfs::register_device("stdout", &tty_ops);
  fs::devfs::register_device("stderr", &tty_ops);
}

} // namespace tty
