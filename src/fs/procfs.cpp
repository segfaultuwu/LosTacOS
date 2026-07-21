#include "LTOS/fs/procfs.hpp"
#include "LTOS/drivers/timer.hpp"
#include "LTOS/fs/vfs.hpp"
#include "LTOS/lib/kprintf.h"
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

void init() {
  kprintf("procfs init\n");

  fs::vfs::create_dir_path("/proc");

  auto *version = fs::vfs::create_file("/proc/version");

  version->file->read = read_version;

  auto *uptime = fs::vfs::create_file("/proc/uptime");

  uptime->file->read = read_uptime;
}

} // namespace fs::procfs
