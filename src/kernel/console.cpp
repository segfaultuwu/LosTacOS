#include "LTOS/console.hpp"

#include "LTOS/fs/vfs.hpp"

namespace console {

static fs::vfs::Node *tty = nullptr;

void init() { tty = fs::vfs::find("/dev/tty"); }

void put(char c) {
  if (!tty)
    return;

  fs::vfs::write(tty, &c, 1);
}

void write(const char *buf, size_t len) {
  if (!tty)
    return;

  fs::vfs::write(tty, buf, len);
}

} // namespace console
