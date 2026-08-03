#include "LTOS/fs/devfs.hpp"

#include "LTOS/drivers/mouse.hpp"
#include "LTOS/fs/vfs.hpp"
#include "LTOS/lib/kprintf.h"
#include "LTOS/logger.hpp"
#include "LTOS/mm/heap.hpp"


#include <string.h>

namespace fs::devfs {

struct Device {
  const char *name;
  vfs::DevOps *ops;

  Device *next;
};

static Device *devices = nullptr;

static size_t kmsg_read(char *buf, size_t len, size_t offset) {
  return logger::read_klog(buf, len, offset);
}

static vfs::DevOps kmsg_ops = {.write = nullptr, .read = kmsg_read, .ioctl = nullptr};

bool init(FileSystem *fs) {
  (void)fs;

  kprintf("devfs initialized\n");
  register_device("kmsg", &kmsg_ops);

  return true;
}

void register_device(const char *name, vfs::DevOps *ops) {
  Device *dev = (Device *)heap::kmalloc(sizeof(Device));
  if (!dev)
    return;

  // Was storing the caller's pointer directly. Fine for a string literal,
  // but ahci.cpp (and potentially other callers) pass a stack-local
  // buffer built with ksnprintf() -- once the enclosing function returns,
  // dev->name (and the vfs node's name below, which shares the same
  // pointer) is left dangling. Whether that manifests as "not found" or
  // as a name that happens to still read back correctly depends entirely
  // on whether something else has reused that stack memory yet, so this
  // could look intermittent. A persistent copy removes the dependency on
  // the caller's storage lifetime entirely.
  char *name_copy = (char *)heap::kmalloc(strlen(name) + 1);
  if (!name_copy) {
    heap::kfree(dev);
    return;
  }
  strcpy(name_copy, name);

  dev->name = name_copy;
  dev->ops = ops;
  dev->next = devices;
  devices = dev;

  vfs::Node *node = vfs::create_dev(name_copy, ops);

  if (!node)
    kprintf("devfs: failed creating /dev/%s\n", name_copy);
}

static Device *find_device(const char *name) {
  Device *d = devices;

  while (d) {
    if (strcmp(d->name, name) == 0)
      return d;

    d = d->next;
  }

  // Dynamic /dev/tty* device creation
  if (strncmp(name, "tty", 3) == 0 && name[3] >= '0' && name[3] <= '9') {
    Device *dev = (Device *)heap::kmalloc(sizeof(Device));
    if (dev) {
      char *name_copy = (char *)heap::kmalloc(strlen(name) + 1);
      strcpy(name_copy, name);
      dev->name = name_copy;
      Device *base_tty = find_device("tty");
      dev->ops = base_tty ? base_tty->ops : nullptr;
      dev->next = devices;
      devices = dev;
      if (dev->ops) {
        vfs::create_dev(name_copy, dev->ops);
      }
      return dev;
    }
  }

  // Dynamic per-monitor /dev/fb* device creation
  if (strncmp(name, "fb", 2) == 0 && name[2] >= '0' && name[2] <= '9') {
    Device *dev = (Device *)heap::kmalloc(sizeof(Device));
    if (dev) {
      char *name_copy = (char *)heap::kmalloc(strlen(name) + 1);
      strcpy(name_copy, name);
      dev->name = name_copy;
      Device *base_fb = find_device("fb");
      if (!base_fb)
        base_fb = find_device("fb0");
      dev->ops = base_fb ? base_fb->ops : nullptr;
      dev->next = devices;
      devices = dev;
      if (dev->ops) {
        vfs::create_dev(name_copy, dev->ops);
      }
      return dev;
    }
  }

  return nullptr;
}

struct DevHandle {
  Device *device;
  size_t offset;
};

static void *open(void *, const char *path) {
  Device *dev = find_device(path);

  if (!dev) {
    kprintf("devfs: open '%s' failed, registered devices:\n", path);
    for (Device *d = devices; d; d = d->next)
      kprintf("devfs:   - '%s'\n", d->name);
    return nullptr;
  }

  DevHandle *h = (DevHandle *)heap::kmalloc(sizeof(DevHandle));

  h->device = dev;
  h->offset = 0;

  return h;
}

static int read(void *file, char *buf, size_t size) {
  DevHandle *h = (DevHandle *)file;

  if (!h || !h->device)
    return -1;

  if (!h->device->ops->read)
    return 0;

  size_t n = h->device->ops->read(buf, size, h->offset);

  h->offset += n;

  return n;
}

static int write(void *file, const char *buf, size_t size) {
  DevHandle *h = (DevHandle *)file;

  if (!h || !h->device)
    return -1;

  if (!h->device->ops->write)
    return 0;

  size_t n = h->device->ops->write(buf, size, h->offset);

  h->offset += n;

  return n;
}

static void close(void *file) {
  heap::kfree(file);
}

static int ioctl(void *file, unsigned long req, void *arg) {
  DevHandle *h = (DevHandle *)file;

  if (!h || !h->device || !h->device->ops->ioctl)
    return -1;

  return h->device->ops->ioctl(req, arg);
}

static void list(void *) {
  Device *d = devices;

  while (d) {
    kprintf("%s\n", d->name);
    d = d->next;
  }
}

static int devfs_lseek(void *file, long offset, int whence) {
  DevHandle *h = (DevHandle *)file;
  if (!h)
    return -1;
  long base = 0;
  if (whence == 0)
    base = 0;
  else if (whence == 1)
    base = (long)h->offset;
  else if (whence == 2)
    base = (long)h->offset;

  long next = base + offset;
  if (next < 0)
    return -1;
  h->offset = (size_t)next;
  return (int)h->offset;
}

FileSystem filesystem = {

    .name = "devfs",

    .init = init,

    .open = open,

    .read = read,

    .write = write,

    .close = close,

    .list = list,

    .ioctl = ioctl,

    .lseek = devfs_lseek

};

} // namespace fs::devfs
