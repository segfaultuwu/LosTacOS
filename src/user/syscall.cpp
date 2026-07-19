#include "LTOS/syscall.hpp"
#include "LTOS/drivers/console.hpp"
#include "LTOS/drivers/keyboard.hpp"
#include "LTOS/fs/vfs.hpp"
#include "LTOS/lib/kprintf.h"
#include "LTOS/sched/scheduler.hpp"
#include <cstddef>
#include <string.h>

namespace {

constexpr int MAX_FDS = 32;
fs::vfs::Node *fd_table[MAX_FDS] = {nullptr};

int alloc_fd(fs::vfs::Node *node) {
  for (int fd = 3; fd < MAX_FDS; fd++) {
    if (!fd_table[fd]) {
      fd_table[fd] = node;
      return fd;
    }
  }

  return -1;
}

bool valid_fd(int fd) {
  return fd >= 3 && fd < MAX_FDS && fd_table[fd] != nullptr;
}

} // namespace

static uint64_t sys_write(uint64_t a, uint64_t b, uint64_t c) {
  int fd = (int)a;
  const char *buf = (const char *)b;
  size_t len = c;

  if (!buf || len == 0)
    return 0;

  if (fd == 1 || fd == 2) {
    console::write(buf, len);
    return len;
  }

  if (!valid_fd(fd))
    return (uint64_t)-1;

  fs::vfs::Node *node = fd_table[fd];

  if (node->dev && node->dev->write)
    return node->dev->write(buf, len);

  // Writing to regular files through an fd isn't supported yet.
  return (uint64_t)-1;
}

static uint64_t sys_read(uint64_t a, uint64_t b, uint64_t c) {
  int fd = (int)a;
  char *buf = (char *)b;
  size_t len = c;

  if (!buf || len == 0)
    return 0;

  if (fd == 0) {
    size_t i = 0;

    while (i < len) {
      char ch = drivers::keyboard::getchar();

      if (ch == '\n') {
        buf[i++] = '\n';
        break;
      }

      if (ch == '\b') {
        if (i)
          i--;
        continue;
      }

      buf[i++] = ch;
    }

    return i;
  }

  if (!valid_fd(fd))
    return (uint64_t)-1;

  fs::vfs::Node *node = fd_table[fd];

  if (node->dev && node->dev->read)
    return node->dev->read(buf, len);

  if (!node->directory && node->file && node->file->private_data) {
    size_t avail = node->file->size;
    size_t n = len < avail ? len : avail;

    memcpy(buf, node->file->private_data, n);

    return n;
  }

  return 0;
}

static uint64_t sys_open(uint64_t a) {
  const char *path = (const char *)a;

  if (!path)
    return (uint64_t)-1;

  fs::vfs::Node *node = fs::vfs::find(path);

  if (!node || node->directory)
    return (uint64_t)-1;

  int fd = alloc_fd(node);

  return fd < 0 ? (uint64_t)-1 : (uint64_t)fd;
}

static uint64_t sys_close(uint64_t a) {
  int fd = (int)a;

  if (!valid_fd(fd))
    return (uint64_t)-1;

  fd_table[fd] = nullptr;

  return 0;
}

static uint64_t sys_exec(uint64_t a) {
  const char *path = (const char *)a;

  if (!path)
    return (uint64_t)-1;

  sched::exec(path);

  return 0;
}

uint64_t syscall_handler(uint64_t num, uint64_t a, uint64_t b, uint64_t c) {
  switch (num) {

  case SYS_WRITE:
    return sys_write(a, b, c);

  case SYS_READ:
    return sys_read(a, b, c);

  case SYS_OPEN:
    return sys_open(a);

  case SYS_CLOSE:
    return sys_close(a);

  case SYS_EXEC:
    return sys_exec(a);

  case SYS_FORK:
    // No address-space duplication support yet.
    kprintf("fork: not supported\n");
    return (uint64_t)-1;

  case SYS_EXIT:
    sched::exit();
    return 0;

  default:
    kprintf("syscall: unknown syscall %lu\n", num);
    return (uint64_t)-1;
  }
}
