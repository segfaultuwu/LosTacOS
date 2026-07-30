#include "LTOS/drivers/tty.hpp"

#include "LTOS/drivers/console.hpp"
#include "LTOS/drivers/serial.hpp"
#include "LTOS/drivers/tty/ioctl.hpp"
#include "LTOS/fs/devfs.hpp"

#include <string.h>

extern volatile size_t stdin_len;
extern char stdin_buffer[256];

namespace tty {

// Line-discipline state. There's only one physical console in LosTacOS, so
// one global termios is all /dev/tty needs -- but it's real state now
// (persisted across ioctl calls), not the zeroed/discarded stand-in that
// used to live in the syscall layer.
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

  tio.c_cc[VERASE] = '\b'; // matches what the keyboard driver emits
  tio.c_cc[VEOF] = 0x04;
  tio.c_cc[VINTR] = 0x03;
  tio.c_cc[VMIN] = 1;
  tio.c_cc[VTIME] = 0;
}

// Pops one byte off the raw keyboard ring buffer. Caller must already know
// stdin_len > 0.
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
      asm volatile("sti; hlt; cli");
      continue;
    }

    char c = pop_raw_byte();

    // Erase actually erases now: it removes the previous character from
    // the line instead of being appended as a literal 0x08 byte (which is
    // what every reader used to receive -- backspace looked fine on
    // screen because console::put() special-cases 0x08, but the string a
    // program got back still had the raw erase byte sitting inside it).
    if (erase && c == erase) {
      if (n > 0) {
        n--;
      }
      continue;
    }

    buf[n++] = c;

    if (c == '\n')
      break;
  }

  return n;
}

// Raw mode: no line editing, no waiting for a newline. Returns as soon as
// there's at least one byte (VMIN == 0 means "don't even block for that").
static size_t read_raw(char *buf, size_t len) {
  bool echo = tio.c_lflag & ECHO;

  if (stdin_len == 0) {
    if (tio.c_cc[VMIN] == 0)
      return 0;

    while (stdin_len == 0)
      asm volatile("sti; hlt; cli");
  }

  size_t n = 0;

  while (n < len && stdin_len > 0) {
    char c = pop_raw_byte();

    buf[n++] = c;
  }

  return n;
}

// The single source of truth for reading from the terminal. Used both for
// fd 0 (stdin) and for anyone who opens /dev/tty directly, so the two can
// no longer drift out of sync like they used to. Honors ICANON/ECHO from
// the current termios instead of always being hard-wired to line-buffered
// echoing input.
size_t read(char *buf, size_t len) {
  ensure_defaults();

  if (tio.c_lflag & ICANON)
    return read_canonical(buf, len);

  return read_raw(buf, len);
}

// Likewise the single source of truth for writing to the terminal, used
// both for fd 1/2 (stdout/stderr) and for /dev/tty writers.
size_t write(const char *buf, size_t len) {
  console::write(buf, len);

  for (size_t i = 0; i < len; i++)
    drivers::serial::write(buf[i]);

  return len;
}

int ioctl(unsigned long req, void *arg) {
  ensure_defaults();

  if (!arg)
    return -1;

  switch (req) {

  case TIOCGWINSZ: {
    auto *ws = (winsize *)arg;

    ws->ws_row = 25;
    ws->ws_col = 80;
    ws->ws_xpixel = 0;
    ws->ws_ypixel = 0;

    return 0;
  }

  case TCGETS: {
    memcpy(arg, &tio, sizeof(termios));
    return 0;
  }

  // No actual serial line to drain/flush, so *W (drain first) and *F
  // (flush input first) collapse to a plain attribute set here.
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

static fs::vfs::DevOps tty_ops = {

    .write = dev_write, .read = dev_read, .ioctl = ioctl

};

void init() {
  ensure_defaults();
  fs::devfs::register_device("tty", &tty_ops);
}

} // namespace tty
