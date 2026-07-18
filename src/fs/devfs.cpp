#include "LTOS/fs/devfs.hpp"
#include "LTOS/fs/vfs.hpp"
#include "LTOS/lib/kprintf.h"

namespace fs::devfs {

static vfs::Node *dev_root = nullptr;

void init() {
  dev_root = vfs::find("/dev");

  if (!dev_root) {
    kprintf("devfs: /dev missing\n");
    return;
  }

  kprintf("devfs initialized\n");
}

void register_device(const char *name, DevOps *ops) {
  fs::vfs::create_dev(name, ops);
}
} // namespace fs::devfs
