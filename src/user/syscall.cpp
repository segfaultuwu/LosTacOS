#include "LTOS/syscall.hpp"
#include "LTOS/drivers/console.hpp"
#include "LTOS/drivers/framebuffer.hpp"
#include "LTOS/drivers/keyboard.hpp"
#include "LTOS/drivers/serial.hpp"
#include "LTOS/drivers/timer.hpp"
#include "LTOS/fs/vfs.hpp"
#include "LTOS/lib/kprintf.h"
#include "LTOS/logger.hpp"
#include "LTOS/sched/process.hpp"
#include "LTOS/sched/scheduler.hpp"
#include "LTOS/sched/task.hpp"
#include <cstddef>
#include <cstdint>
#include <string.h>

namespace {

constexpr int MAX_FDS = 32;

struct FdEntry {
  fs::vfs::Node *node = nullptr;
  size_t offset = 0;
};

FdEntry fd_table[MAX_FDS];

int alloc_fd(fs::vfs::Node *node) {
  for (int fd = 3; fd < MAX_FDS; fd++) {
    if (!fd_table[fd].node) {
      fd_table[fd].node = node;
      fd_table[fd].offset = 0;
      return fd;
    }
  }

  return -1;
}

bool valid_fd(int fd) {
  return fd >= 3 && fd < MAX_FDS && fd_table[fd].node != nullptr;
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

    for (size_t i = 0; i < len; i++)
      drivers::serial::write(buf[i]);

    return len;
  }

  if (!valid_fd(fd))
    return (uint64_t)-1;

  fs::vfs::Node *node = fd_table[fd].node;

  if (node->dev && node->dev->write)
    return node->dev->write(buf, len);

  // idk why it does not render without it, it's already in console::write lol
  framebuffer::swap();

  // Writing to regular files through an fd isn't supported yet.
  return (uint64_t)-1;
}

extern volatile size_t stdin_len;
extern char stdin_buffer[256];

static uint64_t sys_read(uint64_t a, uint64_t b, uint64_t c) {
  int fd = a;
  char *buf = (char *)b;
  size_t len = c;

  if (fd == 0) {
    size_t n = 0;

    while (n < len) {
      if (stdin_len == 0) {
        asm volatile("hlt");
        continue;
      }

      buf[n++] = stdin_buffer[0];

      for (size_t i = 1; i < stdin_len; i++)
        stdin_buffer[i - 1] = stdin_buffer[i];

      stdin_len--;

      char echoed = buf[n - 1];

      console::put_swap(echoed);
      drivers::serial::write(echoed);

      if (echoed == '\n')
        break;
    }

    return n;
  }

  if (!valid_fd(fd))
    return (uint64_t)-1;

  FdEntry &entry = fd_table[fd];
  fs::vfs::Node *node = entry.node;

  if (node->dev && node->dev->read)
    return node->dev->read(buf, len);

  if (!node->directory && node->file && node->file->private_data) {
    size_t size = node->file->size;

    if (entry.offset >= size)
      return 0;

    size_t remaining = size - entry.offset;
    size_t n = len < remaining ? len : remaining;

    memcpy(buf, (const char *)node->file->private_data + entry.offset, n);

    entry.offset += n;

    return n;
  }

  return (uint64_t)-1;
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

  fd_table[fd].node = nullptr;
  fd_table[fd].offset = 0;

  return 0;
}

static uint64_t sys_exec(uint64_t a) {
  const char *path = (const char *)a;

  if (!path)
    return (uint64_t)-1;

  sched::exec(path);

  return 0;
}

static uint64_t sys_getpid() {
  sched::Task *task = sched::get_current();

  return task ? task->pid : (uint64_t)-1;
}

static uint64_t sys_yield() {
  sched::yield();

  return 0;
}

static uint64_t sys_sleep(uint64_t ms) {
  timer::sleep(ms);

  return 0;
}

// whence: 0 = SEEK_SET, 1 = SEEK_CUR, 2 = SEEK_END
static uint64_t sys_lseek(uint64_t a, uint64_t b, uint64_t c) {
  int fd = (int)a;
  int64_t offset = (int64_t)b;
  int whence = (int)c;

  if (!valid_fd(fd))
    return (uint64_t)-1;

  FdEntry &entry = fd_table[fd];
  fs::vfs::Node *node = entry.node;

  size_t size = node->file ? node->file->size : 0;

  int64_t base;

  switch (whence) {
  case 0:
    base = 0;
    break;
  case 1:
    base = (int64_t)entry.offset;
    break;
  case 2:
    base = (int64_t)size;
    break;
  default:
    return (uint64_t)-1;
  }

  int64_t new_offset = base + offset;

  if (new_offset < 0)
    return (uint64_t)-1;

  entry.offset = (size_t)new_offset;

  return entry.offset;
}

static uint64_t sys_fsize(uint64_t a) {
  int fd = (int)a;

  if (!valid_fd(fd))
    return (uint64_t)-1;

  fs::vfs::Node *node = fd_table[fd].node;

  return node->file ? node->file->size : 0;
}

uint64_t sys_fork() {
  sched::Task *parent_task = sched::get_current();

  if (!parent_task)
    return (uint64_t)-1;

  sched::Process *parent = parent_task->process;

  if (!parent)
    return (uint64_t)-1;

  sched::Process *child = sched::clone(parent);

  if (!child)
    return (uint64_t)-1;

  if (child->main_thread && child->main_thread->regs) {
    child->main_thread->regs->rax = 0;
  }

  sched::add(child->main_thread);

  return child->pid;
}

uint64_t syscall_handler(uint64_t num, uint64_t a, uint64_t b, uint64_t c) {
  // kprintf("SYSCALL %lu a=%lx b=%lx c=%lx\n", num, a, b, c);

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
    return sys_fork();

  case SYS_EXIT:
    sched::exit();
    return 0;

  case SYS_GETPID:
    return sys_getpid();

  case SYS_YIELD:
    return sys_yield();

  case SYS_SLEEP:
    return sys_sleep(a);

  case SYS_LSEEK:
    return sys_lseek(a, b, c);

  case SYS_FSIZE:
    return sys_fsize(a);

  default:
    kprintf("syscall: unknown syscall %lu\n", num);
    return (uint64_t)-1;
  }
}
