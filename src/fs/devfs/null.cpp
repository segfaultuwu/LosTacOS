#include "LTOS/fs/devfs.hpp"

namespace fs::devfs {

static size_t null_write(const char *buf, size_t len, size_t offset) {
  (void)buf;
  (void)offset;

  return len;
}

static size_t null_read(char *buf, size_t len, size_t offset) {
  (void)buf;
  (void)len;
  (void)offset;

  return 0;
}

static vfs::DevOps null_ops = {.write = null_write, .read = null_read, .ioctl = nullptr};

void init_null() {
  register_device("null", &null_ops);
}

} // namespace fs::devfs
