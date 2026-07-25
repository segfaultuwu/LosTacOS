#include "LTOS/fs/procfs.hpp"
#include "LTOS/drivers/timer.hpp"
#include "LTOS/fs/vfs.hpp"
#include "LTOS/lib/kprintf.h"
#include "LTOS/mm/heap.hpp"
#include "LTOS/sched/scheduler.hpp"
#include "LTOS_gen/version.h"

#include <cstdint>
#include <string.h>

namespace fs::procfs {

static int read_string(fs::vfs::File *file, uint8_t *buffer, size_t size, const char *text) {
  size_t len = strlen(text);

  if (file->offset >= len)
    return 0;

  size_t remaining = len - file->offset;

  size_t n = remaining < size ? remaining : size;

  memcpy(buffer, text + file->offset, n);

  file->offset += n;

  return n;
}

static int read_version(fs::vfs::File *file, uint8_t *buffer, size_t size) {
  return read_string(file, buffer, size, LTOS_VERSION "\n");
}

static int read_uptime(fs::vfs::File *file, uint8_t *buffer, size_t size) {
  static char uptime[64];

  uint64_t sec = timer::get_uptime_sec();

  int len = ksnprintf(uptime, sizeof(uptime), "%lu\n", sec);

  if (file->offset >= (size_t)len)
    return 0;

  size_t n = len - file->offset;

  if (n > size)
    n = size;

  memcpy(buffer, uptime + file->offset, n);

  file->offset += n;

  return n;
}

static fs::vfs::Node *make_proc_file(const char *path,
                                     int (*read_fn)(fs::vfs::File *, uint8_t *, size_t)) {
  auto *node = fs::vfs::create_file_path(path);

  if (!node) {
    kprintf("procfs: failed to create %s\n", path);
    return nullptr;
  }

  node->file = (fs::vfs::File *)heap::kmalloc(sizeof(fs::vfs::File));

  node->file->size = 0;
  node->file->offset = 0;
  node->file->private_data = nullptr;
  node->file->read = read_fn;
  node->file->write = nullptr;

  return node;
}

void init() {
  kprintf("procfs init\n");

  fs::vfs::create_dir_path("/proc");

  make_proc_file("/proc/version", read_version);

  make_proc_file("/proc/uptime", read_uptime);
}

} // namespace fs::procfs
