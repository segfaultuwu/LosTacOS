#include "LTOS/fs/sysfs.hpp"
#include "LTOS/fs/fs.hpp"
#include "LTOS/fs/vfs.hpp"
#include "LTOS/lib/kprintf.h"
#include "LTOS/mm/heap.hpp"
#include "LTOS_gen/version.h"

#include <string.h>

namespace fs::sysfs {

struct SysEntry {
  const char *path;
  const char *(*generate)();
};

static char buffer[1024];

static const char *get_hostname() {
  return "lostacos\n";
}

static const char *get_osrelease() {
  return LTOS_VERSION "\n";
}

static const char *get_ostype() {
  return "LosTacOS\n";
}

static const char *get_arch() {
  return "x86_64\n";
}

static const char *get_kernel() {
  return "LosTacOS Kernel\n";
}

static SysEntry entries[] = {{"kernel/hostname", get_hostname},
                             {"kernel/osrelease", get_osrelease},
                             {"kernel/ostype", get_ostype},
                             {"kernel/arch", get_arch},
                             {"kernel/name", get_kernel},

                             {nullptr, nullptr}};

struct SysFile {
  char *data;
  size_t size;
  size_t offset;
};

static void *open(void *, const char *path) {
  for (int i = 0; entries[i].path; i++) {
    if (strcmp(path, entries[i].path) == 0) {
      const char *generated = entries[i].generate();

      SysFile *file = (SysFile *)heap::kmalloc(sizeof(SysFile));

      if (!file)
        return nullptr;

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
  SysFile *file = (SysFile *)ptr;

  if (!file || !buf)
    return -1;

  if (file->offset >= file->size)
    return 0;

  size_t left = file->size - file->offset;

  size_t amount = size < left ? size : left;

  memcpy(buf, file->data + file->offset, amount);

  file->offset += amount;

  return amount;
}

static void close(void *ptr) {
  SysFile *file = (SysFile *)ptr;

  if (!file)
    return;

  heap::kfree(file->data);

  heap::kfree(file);
}

static void list(void *) {
  for (int i = 0; entries[i].path; i++) {
    kprintf("%s\n", entries[i].path);
  }
}

static bool init(fs::FileSystem *) {
  kprintf("sysfs init\n");

  vfs::Node *sys = vfs::find("/sys");

  if (!sys) {
    sys = vfs::create_dir_path("/sys");
  }

  if (sys) {
    vfs::Node *kernel = vfs::create_node("kernel", true, sys);

    if (kernel) {
      vfs::create_node("hostname", false, kernel);

      vfs::create_node("osrelease", false, kernel);

      vfs::create_node("ostype", false, kernel);

      vfs::create_node("arch", false, kernel);

      vfs::create_node("name", false, kernel);
    }
  }

  return true;
}

FileSystem filesystem = {.name = "sysfs",

                         .init = init,

                         .open = open,

                         .read = read,

                         .write = nullptr,

                         .close = close,

                         .list = list};

} // namespace fs::sysfs
