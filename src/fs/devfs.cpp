#include "LTOS/fs/devfs.hpp"

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

  dev->name = name;
  dev->ops = ops;
  dev->next = devices;
  devices = dev;

  vfs::Node *node = vfs::create_dev(name, ops);

  if (!node)
    kprintf("devfs: failed creating /dev/%s\n", name);

  kprintf("devfs: registered /dev/%s\n", name);
}

static Device *find_device(const char *name) {
  Device *d = devices;

  while (d) {
    if (strcmp(d->name, name) == 0)
      return d;

    d = d->next;
  }

  return nullptr;
}

struct DevHandle {
  Device *device;
  size_t offset;
};

static void *open(void *, const char *path) {
  Device *dev = find_device(path);

  if (!dev)
    return nullptr;

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

FileSystem filesystem = {

    .name = "devfs",

    .init = init,

    .open = open,

    .read = read,

    .write = write,

    .close = close,

    .list = list,

    .ioctl = ioctl

};

} // namespace fs::devfs
