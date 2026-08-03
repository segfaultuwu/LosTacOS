#include "LTOS/fs/sysfs.hpp"

#include "LTOS/boot.hpp"
#include "LTOS/drivers/framebuffer.hpp"
#include "LTOS/fs/fs.hpp"
#include "LTOS/fs/vfs.hpp"
#include "LTOS/lib/kprintf.h"
#include "LTOS/mm/heap.hpp"
#include "LTOS/mm/pmm.hpp"

#include "LTOS_gen/version.h"

#include <string.h>

namespace fs::sysfs {

struct SysEntry {
  const char *path;
  const char *(*generate)();
};

static const char *get_hostname() {
  return "lostacos\n";
}

static const char *get_osrelease() {
  return LTOS_VERSION "\n";
}

static const char *get_ostype() {
  return "Unix-like/LosTacOS\n";
}

static const char *get_arch() {
  return "x86_64\n";
}

static const char *get_kernel() {
  return "LosTacOS Kernel\n";
}

static const char *get_bootloader() {
  static char buffer[128];

  ksnprintf(buffer, sizeof(buffer), "%s\n", boot_info.bootloader_name);

  return buffer;
}

static const char *get_framebuffer() {
  static char buffer[256];

  ksnprintf(buffer, sizeof(buffer),
            "address=%lx\n"
            "width=%u\n"
            "height=%u\n"
            "pitch=%u\n"
            "bpp=%u\n",
            (uint64_t)framebuffer::info.address, framebuffer::info.width, framebuffer::info.height,
            framebuffer::info.pitch, framebuffer::info.bpp);

  return buffer;
}

static const char *get_memory() {
  static char buffer[256];

  ksnprintf(buffer, sizeof(buffer),
            "total=%lu MB\n"
            "free=%lu MB\n"
            "used=%lu MB\n",
            pmm::total_memory() / 1024 / 1024, pmm::free_memory() / 1024 / 1024,
            pmm::used_memory() / 1024 / 1024);

  return buffer;
}

static const char *get_cpu() {
  return "architecture=x86_64\n";
}

static SysEntry entries[] = {{"kernel/hostname", get_hostname},

                             {"kernel/osrelease", get_osrelease},

                             {"kernel/ostype", get_ostype},

                             {"kernel/arch", get_arch},

                             {"kernel/name", get_kernel},

                             {"firmware/bootloader", get_bootloader},

                             {"firmware/framebuffer", get_framebuffer},

                             {"memory/info", get_memory},

                             {"cpu/arch", get_cpu},

                             {nullptr, nullptr}};

struct SysFile {
  bool directory;
  char *data;
  size_t size;
  size_t offset;
};

static void *open(void *node_ptr, const char *path) {
  vfs::Node *node = (vfs::Node *)node_ptr;

  if (node && node->directory) {
    SysFile *file = (SysFile *)heap::kmalloc(sizeof(SysFile));

    if (!file)
      return nullptr;

    file->directory = true;
    file->data = nullptr;
    file->size = 0;
    file->offset = 0;

    return file;
  }

  for (int i = 0; entries[i].path; i++) {
    if (strcmp(path, entries[i].path) == 0) {
      const char *generated = entries[i].generate();

      SysFile *file = (SysFile *)heap::kmalloc(sizeof(SysFile));

      if (!file)
        return nullptr;

      file->directory = false;

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

  if (!file || file->directory)
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

  if (!file->directory && file->data)
    heap::kfree(file->data);

  heap::kfree(file);
}

static void list(void *ptr) {
  const char *dir = (const char *)ptr;

  for (int i = 0; entries[i].path; i++) {
    const char *p = entries[i].path;

    if (strncmp(p, dir, strlen(dir)) == 0) {
      p += strlen(dir);

      if (*p == '/')
        p++;

      if (strchr(p, '/') == nullptr)
        kprintf("%s\n", p);
    }
  }
}
static bool init(fs::FileSystem *) {

  vfs::Node *sys = vfs::find("/sys");

  if (!sys)
    sys = vfs::create_dir_path("/sys");

  if (!sys)
    return false;

  vfs::Node *kernel = vfs::create_node("kernel", true, sys);

  vfs::Node *firmware = vfs::create_node("firmware", true, sys);

  vfs::Node *memory = vfs::create_node("memory", true, sys);

  vfs::Node *cpu = vfs::create_node("cpu", true, sys);

  if (kernel) {
    vfs::create_node("hostname", false, kernel);

    vfs::create_node("osrelease", false, kernel);

    vfs::create_node("ostype", false, kernel);

    vfs::create_node("arch", false, kernel);

    vfs::create_node("name", false, kernel);
  }

  if (firmware) {
    vfs::create_node("bootloader", false, firmware);

    vfs::create_node("framebuffer", false, firmware);
  }

  if (memory) {
    vfs::create_node("info", false, memory);
  }

  if (cpu) {
    vfs::create_node("arch", false, cpu);
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
