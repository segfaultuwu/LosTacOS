#include "LTOS/syscall.hpp"
#include "LTOS/arch/x86_64/paging.hpp"
#include "LTOS/drivers/console.hpp"
#include "LTOS/drivers/framebuffer.hpp"
#include "LTOS/drivers/serial.hpp"
#include "LTOS/drivers/timer.hpp"
#include "LTOS/drivers/tty.hpp"
#include "LTOS/drivers/tty/ioctl.hpp"
#include "LTOS/fs/vfs.hpp"
#include "LTOS/lib/kprintf.h"
#include "LTOS/mm/heap.hpp"
#include "LTOS/sched/process.hpp"
#include "LTOS/sched/scheduler.hpp"
#include "LTOS/sched/task.hpp"
#include "sys/time.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace {

constexpr int MAX_FDS = 32;

enum FDType { FD_NONE, FD_FILE, FD_PIPE };

struct Pipe {
  char buffer[4096];

  size_t read_pos;
  size_t write_pos;

  int readers;
  int writers;
};

struct FdEntry {
  FDType type = FD_NONE;

  fs::vfs::Node *node = nullptr;

  Pipe *pipe = nullptr;

  fs::Mount *mount = nullptr;
  void *fs_handle = nullptr;

  size_t offset = 0;

  bool readable = false;
  bool writable = false;
};

static Pipe pipes[32];
static bool pipe_used[32];

FdEntry fd_table[MAX_FDS];

int alloc_fd(fs::vfs::Node *node) {
  for (int fd = 3; fd < MAX_FDS; fd++) {

    if (fd_table[fd].type == FD_NONE) {

      memset(&fd_table[fd], 0, sizeof(FdEntry));

      fd_table[fd].node = node;

      if (node)
        fd_table[fd].type = FD_FILE;

      return fd;
    }
  }

  return -1;
}

bool valid_fd(int fd) {
  return fd >= 0 && fd < MAX_FDS && fd_table[fd].type != FD_NONE;
}

} // namespace

static uint64_t sys_pipe(uint64_t arg) {
  int *fds = (int *)arg;

  if (!fds)
    return -1;

  for (int i = 0; i < 32; i++) {
    if (!pipe_used[i]) {
      pipe_used[i] = true;

      Pipe *p = &pipes[i];

      memset(p, 0, sizeof(Pipe));

      p->readers = 1;
      p->writers = 1;

      int rfd = -1;
      int wfd = -1;

      for (int j = 3; j < MAX_FDS; j++) {
        if (fd_table[j].type == FD_NONE) {
          rfd = j;
          break;
        }
      }

      for (int j = 3; j < MAX_FDS; j++) {
        if (fd_table[j].type == FD_NONE && j != rfd) {
          wfd = j;
          break;
        }
      }

      if (rfd < 0 || wfd < 0)
        return -1;

      fd_table[rfd].type = FD_PIPE;
      fd_table[rfd].pipe = p;
      fd_table[rfd].readable = true;
      fd_table[rfd].writable = false;

      fd_table[wfd].type = FD_PIPE;
      fd_table[wfd].pipe = p;
      fd_table[wfd].readable = false;
      fd_table[wfd].writable = true;

      fds[0] = rfd;
      fds[1] = wfd;

      return 0;
    }
  }

  return -1;
}

static uint64_t sys_write(uint64_t a, uint64_t b, uint64_t c) {
  int fd = (int)a;
  const char *buf = (const char *)b;
  size_t len = c;

  if (!buf || len == 0)
    return 0;

  if (fd == 1 || fd == 2)
    return tty::write(buf, len);

  if (!valid_fd(fd))
    return (uint64_t)-1;

  FdEntry &entry = fd_table[fd];

  if (entry.type == FD_PIPE) {
    if (!entry.writable)
      return -1;

    Pipe *p = entry.pipe;

    size_t written = 0;

    while (written < len) {
      if (p->write_pos >= sizeof(p->buffer))
        break;

      p->buffer[p->write_pos++] = buf[written++];
    }

    return written;
  }

  fs::vfs::Node *node = fd_table[fd].node;

  if (node->dev && node->dev->write)
    return node->dev->write(buf, len, fd_table[fd].offset);

  // idk why it does not render without it, it's already in console::write lol
  framebuffer::swap();

  return (uint64_t)-1;
}

static uint64_t sys_read(uint64_t a, uint64_t b, uint64_t c) {
  int fd = a;
  char *buf = (char *)b;
  size_t len = c;

  if (fd == 0)
    return tty::read(buf, len);

  if (!valid_fd(fd))
    return (uint64_t)-1;

  FdEntry &entry = fd_table[fd];

  if (!entry.node && entry.mount && entry.fs_handle) {

    return entry.mount->fs->read(entry.fs_handle, buf, len);
  }

  if (entry.type == FD_PIPE) {
    if (!entry.readable)
      return -1;

    Pipe *p = entry.pipe;

    while (p->read_pos >= p->write_pos) {
      asm volatile("sti; hlt; cli");
    }

    size_t n = 0;

    while (n < len && p->read_pos < p->write_pos) {
      buf[n++] = p->buffer[p->read_pos++];
    }

    return n;
  }

  fs::vfs::Node *node = entry.node;

  if (node->dev && node->dev->read)
    return node->dev->read(buf, len, fd_table[fd].offset);

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

  fs::Mount *mnt = fs::find_mount(path);

  if (mnt) {
    const char *sub = path + strlen(mnt->path);

    while (*sub == '/')
      sub++;

    if (*sub) {
      void *handle = mnt->fs->open(mnt->data, sub);

      if (handle) {
        int fd = alloc_fd(nullptr);

        if (fd >= 0) {
          fd_table[fd].type = FD_FILE;
          fd_table[fd].fs_handle = handle;
          fd_table[fd].mount = mnt;
          return fd;
        }
      }

      return -1;
    }

    // opening the mount point itself (e.g. "/dev", "/proc") -- fall
    // through to the plain vfs node below so directory listing works.
  }

  fs::vfs::Node *node = fs::vfs::find(path);

  if (!node)
    return -1;

  return alloc_fd(node);
}

static uint64_t sys_close(uint64_t a) {
  int fd = a;

  if (!valid_fd(fd))
    return -1;

  FdEntry &entry = fd_table[fd];

  if (entry.type == FD_PIPE) {
    if (entry.readable)
      entry.pipe->readers--;

    if (entry.writable)
      entry.pipe->writers--;

    if (entry.pipe->readers == 0 && entry.pipe->writers == 0) {
      for (int i = 0; i < 32; i++) {
        if (&pipes[i] == entry.pipe)
          pipe_used[i] = false;
      }
    }
  }

  memset(&entry, 0, sizeof(FdEntry));

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

  if (entry.type == FD_PIPE)
    return -1;

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

  if (fd_table[fd].type == FD_PIPE)
    return 0;

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

static uint64_t sys_dup2(uint64_t a, uint64_t b) {
  int oldfd = a;
  int newfd = b;

  if (!valid_fd(oldfd))
    return -1;

  if (newfd < 0 || newfd >= MAX_FDS)
    return -1;

  fd_table[newfd] = fd_table[oldfd];

  return newfd;
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

  if (!dir || !dir->directory)
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
    sched::exit(a);
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

  case SYS_PIPE:
    return sys_pipe(a);

  case SYS_DUP2:
    return sys_dup2(a, b);

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
