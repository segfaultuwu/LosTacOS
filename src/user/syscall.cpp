#include "LTOS/syscall.hpp"
#include "LTOS/arch/x86_64/paging.hpp"
#include "LTOS/drivers/console.hpp"
#include "LTOS/drivers/framebuffer.hpp"
#include "LTOS/drivers/serial.hpp"
#include "LTOS/drivers/timer.hpp"
#include "LTOS/drivers/tty/ioctl.hpp"
#include "LTOS/fs/vfs.hpp"
#include "LTOS/lib/kprintf.h"
#include "LTOS/mm/heap.hpp"
#include "LTOS/sched/process.hpp"
#include "LTOS/sched/scheduler.hpp"
#include "LTOS/sched/task.hpp"
#include "sys/time.h"
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
        asm volatile("sti; hlt; cli");
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

  if (!node->directory && node->file && node->file->read) {
    node->file->offset = entry.offset;

    int n = node->file->read(node->file, (uint8_t *)buf, len);

    entry.offset = node->file->offset;

    return n;
  }

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

  if (!node)
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

static uint64_t sys_execve(uint64_t a, uint64_t b, uint64_t c) {
  const char *path = (const char *)a;
  char **argv = (char **)b;
  char **envp = (char **)c;

  // On success exec_current() never returns (it jumps straight into the new
  // program via exec_enter()). It only returns here on failure, so that's
  // the only case we need to convert -- and it must come back as -1, not 0,
  // or callers checking `execve(...) < 0` will think it succeeded.
  if (!sched::exec_current(path, argv, envp))
    return (uint64_t)-1;

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

static uint64_t sys_wait(uint64_t a) {
  uint64_t pid = a;

  while (true) {
    sched::Task *task = sched::find(pid);

    if (!task || task->state == sched::State::DEAD)
      return 0;

    asm volatile("sti; hlt; cli");
  }
}

static uint64_t sys_fork() {
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

  return child->pid;
}

static uint64_t sys_mmap(uint64_t a, uint64_t b) {
  (void)a; // requested address (arg1) -- ignored, we always pick the address

  size_t len = (size_t)b; // arg2: length

  if (len == 0)
    return 0;

  sched::Task *task = sched::get_current();

  if (!task || !task->process || !task->process->space || !task->process->space->table)
    return 0;

  sched::Process *proc = task->process;

  // Bump-allocate out of the private per-process region (PDPT slot 1,
  // 0x40000000-0x80000000 -- see paging.cpp) that elf::load() places the
  // program's own image in. MMAP_BASE sits well above where any binary
  // built with cfg/user.ld is going to reach, so it won't collide with
  // the program image or its BSS.
  constexpr uint64_t MMAP_BASE = 0x50000000;

  if (!proc->mmap_next)
    proc->mmap_next = MMAP_BASE;

  uint64_t base = proc->mmap_next;
  uint64_t pages = (len + 0xFFF) / 0x1000;

  for (uint64_t i = 0; i < pages; i++) {
    void *page = paging::alloc_page(); // zeroed already

    if (!page)
      return 0;

    paging::map_page(proc->space->table->pml4, proc->mmap_next, (uint64_t)page,
                     PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);

    proc->mmap_next += 0x1000;
  }

  return base;
}

static uint64_t sys_munmap(uint64_t ptr) {
  if (!ptr)
    return -1;

  // sys_mmap() now hands out pages mapped straight into the caller's own
  // address space rather than a kernel-heap pointer, and nothing in this
  // kernel reclaims individual physical pages once handed out (alloc_page()
  // itself is a bump allocator with no free path either). So there is
  // nothing safe to do here yet beyond accepting the call -- freeing the
  // mapping would need a per-process allocator that can hand pages back,
  // which doesn't exist. Leaving this a no-op avoids treating a user VA as
  // a kernel heap pointer, which is what caused the previous crash.
  return 0;
}

static uint64_t sys_readdir(uint64_t a, uint64_t b) {
  int fd = (int)a;
  auto *out = (fs::vfs::Dirent *)b;

  if (!valid_fd(fd) || !out)
    return (uint64_t)-1;

  auto &entry = fd_table[fd];

  auto *dir = entry.node;

  if (!dir->directory)
    return (uint64_t)-1;

  auto *child = dir->children;

  size_t index = 0;

  while (child) {
    if (index == entry.offset) {
      memset(out, 0, sizeof(*out));

      out->d_ino = 0;
      out->d_off = entry.offset + 1;

      out->d_reclen = sizeof(*out);

      out->d_type = child->directory ? fs::vfs::DT_DIR : fs::vfs::DT_REG;

      strncpy(out->d_name, child->name, sizeof(out->d_name) - 1);

      entry.offset++;

      return 1;
    }

    index++;
    child = child->next;
  }

  return 0;
}

static uint64_t sys_ioctl(uint64_t a, uint64_t b, uint64_t c) {
  int fd = a;
  unsigned long req = b;
  void *arg = (void *)c;

  if (req == TIOCGWINSZ) {
    auto *ws = (winsize *)arg;

    ws->ws_row = 25;
    ws->ws_col = 80;

    ws->ws_xpixel = 0;
    ws->ws_ypixel = 0;

    return 0;
  }

  if (req == TCGETS) {
    auto *term = (termios *)arg;

    memset(term, 0, sizeof(termios));

    return 0;
  }

  if (req == TCSETS) {
    // here later raw mode
    return 0;
  }

  return -1;
}

static uint64_t sys_gettimeofday(uint64_t a) {
  auto *tv = (timeval *)a;

  if (!tv)
    return -1;

  uint64_t ms = timer::ticks();

  tv->tv_sec = ms / 1000;
  tv->tv_usec = (ms % 1000) * 1000;

  return 0;
}

extern "C" uint64_t syscall_handler(uint64_t num, uint64_t a, uint64_t b, uint64_t c, uint64_t d,
                                    uint64_t e, uint64_t f) {
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

  case SYS_EXECVE:
    return sys_execve(a, b, c);

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

  case SYS_WAIT4:
    return sys_wait(a);

  case SYS_NANOSLEEP:
    return sys_sleep(a);

  case SYS_IOCTL:
    return sys_ioctl(a, b, c);

  case SYS_MMAP:
    return sys_mmap(a, b);

  case SYS_MUNMAP:
    return sys_munmap(a);

  case SYS_READDIR:
    return sys_readdir(a, b);

  case SYS_GETTIMEOFDAY:
    return sys_gettimeofday(a);

  default:
    kprintf("syscall: unknown syscall %lu\n", num);
    return (uint64_t)-1;
  }
}
