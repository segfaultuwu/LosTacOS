#include "LTOS/fs/procfs.hpp"
#include "LTOS/drivers/timer.hpp"
#include "LTOS/fs/fs.hpp"
#include "LTOS/fs/vfs.hpp"
#include "LTOS/lib/kprintf.h"
#include "LTOS/mm/heap.hpp"
#include "LTOS_gen/version.h"

#include <string.h>

namespace fs::procfs {

struct ProcEntry {
  const char *name;
  const char *(*generate)();
};

static char buffer[128];

static const char *get_version() {
  return "LosTacOS v" LTOS_VERSION;
}

static const char *get_uptime() {
  ksnprintf(buffer, sizeof(buffer), "%lu", timer::get_uptime_ms());

  return buffer;
}

static ProcEntry entries[] = {{"version", get_version}, {"uptime", get_uptime}, {nullptr, nullptr}};

struct ProcFile {
  char *data;
  size_t offset;
  size_t size;
};

static void *open(void *, const char *path) {
  for (int i = 0; entries[i].name; i++) {

    if (strcmp(path, entries[i].name) == 0) {

      ProcFile *file = (ProcFile *)heap::kmalloc(sizeof(ProcFile));

      if (!file)
        return nullptr;

      const char *generated = entries[i].generate();

      size_t len = strlen(generated);

      file->data = (char *)heap::kmalloc(len + 1);

      if (!file->data) {
        heap::kfree(file);
        return nullptr;
      }

      memcpy(file->data, generated, len);
      file->data[len] = 0;

      file->size = len;
      file->offset = 0;

      return file;
    }
  }

  return nullptr;
}

static int read(void *ptr, char *buf, size_t size) {
  ProcFile *file = (ProcFile *)ptr;

  if (!file || !buf)
    return -1;

  if (file->offset >= file->size)
    return 0;

  size_t remaining = file->size - file->offset;

  size_t n = size < remaining ? size : remaining;

  memcpy(buf, file->data + file->offset, n);

  file->offset += n;

  return n;
}

static void close(void *ptr) {
  ProcFile *file = (ProcFile *)ptr;

  if (!file)
    return;

  heap::kfree(file->data);
  heap::kfree(file);
}

static void list(void *) {
  for (int i = 0; entries[i].name; i++)
    kprintf("%s\n", entries[i].name);
}

static bool init(fs::FileSystem *fs) {
  kprintf("procfs init\n");

  // Give /proc real vfs children for each entry (mirroring how devfs
  // registers a node per device) so directory listing works: readdir
  // walks node->children, and until now /proc never had any.
  vfs::Node *proc_dir = vfs::find("/proc");

  if (proc_dir) {
    for (int i = 0; entries[i].name; i++)
      vfs::create_node(entries[i].name, false, proc_dir);
  }

  return true;
}

FileSystem filesystem = {

    .name = "procfs",

    .init = init,

    .open = open,

    .read = read,

    .write = nullptr,

    .close = close,

    .list = list};

} // namespace fs::procfs
