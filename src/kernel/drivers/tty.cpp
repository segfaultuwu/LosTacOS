#include "LTOS/drivers/tty.hpp"

#include "LTOS/console.hpp"
#include "LTOS/fs/devfs.hpp"

namespace tty {

static size_t tty_write(const char *buf, size_t len) {
  for (size_t i = 0; i < len; i++)
    console::put(buf[i]);

  return len;
}

static size_t tty_read(char *buf, size_t len) {
  (void)buf;
  (void)len;

  return 0;
}

static fs::devfs::DevOps tty_ops = {.write = tty_write, .read = tty_read};

void init() { fs::devfs::register_device("tty", &tty_ops); }

void write(const char *buf, size_t len) { tty_write(buf, len); }

size_t read(char *buf, size_t len) { return tty_read(buf, len); }

} // namespace tty
