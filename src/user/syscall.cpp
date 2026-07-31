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

using sched::FDType;
using sched::Pipe;
using sched::FdEntry;
using sched::FD_NONE;
using sched::FD_FILE;
using sched::FD_PIPE;

static Pipe pipes[32];
static bool pipe_used[32];

static FdEntry *get_fd_entry(int fd) {
  if (fd < 0 || fd >= MAX_FDS)
    return nullptr;

  sched::Task *task = sched::get_current();
  if (!task || !task->process)
    return nullptr;

  return &task->process->fds[fd];
}

int alloc_fd(fs::vfs::Node *node) {
  sched::Task *task = sched::get_current();
  if (!task || !task->process)
    return -1;

  sched::Process *proc = task->process;

  for (int fd = 3; fd < MAX_FDS; fd++) {

    if (proc->fds[fd].type == FD_NONE) {

      memset(&proc->fds[fd], 0, sizeof(FdEntry));

      proc->fds[fd].node = node;

      if (node)
        proc->fds[fd].type = FD_FILE;

      return fd;
    }
  }

  return -1;
}

bool valid_fd(int fd) {
  FdEntry *entry = get_fd_entry(fd);
  return entry != nullptr && entry->type != FD_NONE;
}

} // namespace

static uint64_t sys_pipe(uint64_t arg) {
  int *fds = (int *)arg;

  if (!fds)
    return -1;

  sched::Task *task = sched::get_current();
  if (!task || !task->process)
    return -1;

  sched::Process *proc = task->process;

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
        if (proc->fds[j].type == FD_NONE) {
          rfd = j;
          break;
        }
      }

      for (int j = 3; j < MAX_FDS; j++) {
        if (proc->fds[j].type == FD_NONE && j != rfd) {
          wfd = j;
          break;
        }
      }

      if (rfd < 0 || wfd < 0)
        return -1;

      proc->fds[rfd].type = FD_PIPE;
      proc->fds[rfd].pipe = p;
      proc->fds[rfd].readable = true;
      proc->fds[rfd].writable = false;

      proc->fds[wfd].type = FD_PIPE;
      proc->fds[wfd].pipe = p;
      proc->fds[wfd].readable = false;
      proc->fds[wfd].writable = true;

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

  FdEntry *entry = get_fd_entry(fd);
  if (!entry || entry->type == FD_NONE)
    return (uint64_t)-1;

  if (entry->type == FD_PIPE) {
    if (!entry->writable)
      return -1;

    Pipe *p = entry->pipe;

    size_t written = 0;

    while (written < len) {
      if (p->write_pos >= sizeof(p->buffer))
        break;

      p->buffer[p->write_pos++] = buf[written++];
    }

    return written;
  }

  fs::vfs::Node *node = entry->node;

  if (node && node->dev && node->dev->write)
    return node->dev->write(buf, len, entry->offset);

  framebuffer::swap();

  return (uint64_t)-1;
}

static uint64_t sys_read(uint64_t a, uint64_t b, uint64_t c) {
  int fd = a;
  char *buf = (char *)b;
  size_t len = c;

  if (fd == 0)
    return tty::read(buf, len);

  FdEntry *entry = get_fd_entry(fd);
  if (!entry || entry->type == FD_NONE)
    return (uint64_t)-1;

  if (!entry->node && entry->mount && entry->fs_handle && entry->mount->fs && entry->mount->fs->read) {
    return entry->mount->fs->read(entry->fs_handle, buf, len);
  }

  if (entry->type == FD_PIPE) {
    if (!entry->readable)
      return -1;

    Pipe *p = entry->pipe;

    while (p->read_pos >= p->write_pos) {
      asm volatile("sti; hlt");
    }

    size_t n = 0;

    while (n < len && p->read_pos < p->write_pos) {
      buf[n++] = p->buffer[p->read_pos++];
    }

    return n;
  }

  fs::vfs::Node *node = entry->node;

  if (node && node->dev && node->dev->read)
    return node->dev->read(buf, len, entry->offset);

  if (node && !node->directory && node->file && node->file->read) {
    node->file->offset = entry->offset;

    int n = node->file->read(node->file, (uint8_t *)buf, len);

    entry->offset = node->file->offset;

    return n;
  }

  if (node && !node->directory && node->file && node->file->private_data) {
    size_t size = node->file->size;

    if (entry->offset >= size)
      return 0;

    size_t remaining = size - entry->offset;
    size_t n = len < remaining ? len : remaining;

    memcpy(buf, (const char *)node->file->private_data + entry->offset, n);

    entry->offset += n;

    return n;
  }

  return (uint64_t)-1;
}

static uint64_t sys_open(uint64_t a) {
  const char *path = (const char *)a;

  fs::Mount *mnt = fs::find_mount(path);

  if (mnt && mnt->fs && mnt->fs->open) {
    const char *sub = path + strlen(mnt->path);

    while (*sub == '/')
      sub++;

    if (*sub) {
      void *handle = mnt->fs->open(mnt->data, sub);

      if (handle) {
        int fd = alloc_fd(nullptr);

        if (fd >= 0) {
          FdEntry *entry = get_fd_entry(fd);
          if (entry) {
            entry->type = FD_FILE;
            entry->fs_handle = handle;
            entry->mount = mnt;
            return fd;
          }
        }
      }

      return -1;
    }
  }

  fs::vfs::Node *node = fs::vfs::find(path);

  if (!node)
    return -1;

  return alloc_fd(node);
}

static uint64_t sys_close(uint64_t a) {
  int fd = a;

  FdEntry *entry = get_fd_entry(fd);
  if (!entry || entry->type == FD_NONE)
    return -1;

  if (entry->mount && entry->fs_handle && entry->mount->fs && entry->mount->fs->close) {
    entry->mount->fs->close(entry->fs_handle);
  }

  if (entry->type == FD_PIPE) {
    if (entry->readable)
      entry->pipe->readers--;

    if (entry->writable)
      entry->pipe->writers--;

    if (entry->pipe->readers == 0 && entry->pipe->writers == 0) {
      for (int i = 0; i < 32; i++) {
        if (&pipes[i] == entry->pipe)
          pipe_used[i] = false;
      }
    }
  }

  memset(entry, 0, sizeof(FdEntry));

  return 0;
}

static uint64_t sys_execve(uint64_t a, uint64_t b, uint64_t c) {
  const char *path = (const char *)a;
  char **argv = (char **)b;
  char **envp = (char **)c;

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

  FdEntry *entry = get_fd_entry(fd);
  if (!entry || entry->type == FD_NONE)
    return (uint64_t)-1;

  if (entry->type == FD_PIPE)
    return -1;

  fs::vfs::Node *node = entry->node;

  size_t size = (node && node->file) ? node->file->size : 0;

  int64_t base;

  switch (whence) {
  case 0:
    base = 0;
    break;
  case 1:
    base = (int64_t)entry->offset;
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

  entry->offset = (size_t)new_offset;

  return entry->offset;
}

static uint64_t sys_fsize(uint64_t a) {
  int fd = (int)a;

  FdEntry *entry = get_fd_entry(fd);
  if (!entry || entry->type == FD_NONE)
    return (uint64_t)-1;

  if (entry->type == FD_PIPE)
    return 0;

  fs::vfs::Node *node = entry->node;

  return (node && node->file) ? node->file->size : 0;
}

static uint64_t sys_wait(uint64_t a) {
  uint64_t pid = a;

  while (true) {
    sched::Task *task = sched::find(pid);

    if (!task || task->state == sched::State::DEAD) {
      if (task) {
        sched::remove_and_destroy(task);
      }
      return 0;
    }

    sched::Task *curr = sched::get_current();
    if (curr)
      curr->state = sched::State::BLOCKED;

    sched::yield();
  }
}

static uint64_t sys_dup2(uint64_t a, uint64_t b) {
  int oldfd = a;
  int newfd = b;

  FdEntry *old_entry = get_fd_entry(oldfd);
  if (!old_entry || old_entry->type == FD_NONE)
    return -1;

  if (newfd < 0 || newfd >= MAX_FDS)
    return -1;

  sched::Task *task = sched::get_current();
  if (task && task->process) {
    task->process->fds[newfd] = *old_entry;
    return newfd;
  }

  return -1;
}

static uint64_t sys_fork() {
  sched::Task *parent_task = sched::get_current();

  if (!parent_task)
    return (uint64_t)-1;

  sched::Process *parent = parent_task->process;

  if (!parent)
    return (uint64_t)-1;

  parent->main_thread = parent_task;

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

  return 0;
}

static uint64_t sys_readdir(uint64_t a, uint64_t b) {
  int fd = (int)a;
  auto *out = (fs::vfs::Dirent *)b;

  FdEntry *entry = get_fd_entry(fd);
  if (!entry || entry->type == FD_NONE || !out)
    return (uint64_t)-1;

  auto *dir = entry->node;

  if (!dir || !dir->directory)
    return (uint64_t)-1;

  auto *child = dir->children;

  size_t index = 0;

  while (child) {
    if (index == entry->offset) {
      memset(out, 0, sizeof(*out));

      out->d_ino = 0;
      out->d_off = entry->offset + 1;

      out->d_reclen = sizeof(*out);

      out->d_type = child->directory ? fs::vfs::DT_DIR : fs::vfs::DT_REG;

      strncpy(out->d_name, child->name, sizeof(out->d_name) - 1);

      entry->offset++;

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
