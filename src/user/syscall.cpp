#include "LTOS/syscall.hpp"
#include "LTOS/arch/x86_64/paging.hpp"
#include "LTOS/drivers/console.hpp"
#include "LTOS/drivers/framebuffer.hpp"
#include "LTOS/drivers/serial.hpp"
#include "LTOS/drivers/timer.hpp"
#include "LTOS/drivers/tty.hpp"
#include "LTOS/drivers/tty/ioctl.hpp"
#include "LTOS/fs/procfs.hpp"
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

using sched::FD_FILE;
using sched::FD_NONE;
using sched::FD_PIPE;
using sched::FdEntry;
using sched::FDType;
using sched::Pipe;

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

  FdEntry *entry = get_fd_entry(fd);

  if ((fd == 1 || fd == 2) && (!entry || entry->type == sched::FD_NONE || entry->type == sched::FD_TTY))
    return tty::write(buf, len);

  if (!entry || entry->type == sched::FD_NONE)
    return (uint64_t)-1;

  if (entry->type == sched::FD_TTY)
    return tty::write(buf, len);

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

  if (node) {
    if (node->dev && node->dev->write)
      return node->dev->write(buf, len, entry->offset);

    if (!node->directory) {
      if (!fs::vfs::ensure_file_storage(node))
        return (uint64_t)-1;

      size_t new_offset = entry->offset + len;
      if (new_offset > node->file->size) {
        char *new_data = (char *)heap::kmalloc(new_offset + 1);
        if (!new_data)
          return (uint64_t)-1;

        if (node->file->private_data) {
          memcpy(new_data, node->file->private_data, node->file->size);
          heap::kfree(node->file->private_data);
        }

        node->file->private_data = new_data;
        node->file->size = new_offset;
      }

      memcpy((char *)node->file->private_data + entry->offset, buf, len);
      ((char *)node->file->private_data)[node->file->size] = '\0';
      entry->offset = new_offset;

      return len;
    }
  }

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

  if (!entry->node && entry->mount && entry->fs_handle && entry->mount->fs &&
      entry->mount->fs->read) {
    return entry->mount->fs->read(entry->fs_handle, buf, len);
  }

  if (entry->type == FD_PIPE) {
    if (!entry->readable)
      return -1;

    Pipe *p = entry->pipe;

    while (p->read_pos >= p->write_pos) {
      if (p->writers <= 0) {
        return 0;
      }
      asm volatile("sti; hlt");
    }

    size_t n = 0;

    while (n < len && p->read_pos < p->write_pos) {
      buf[n++] = p->buffer[p->read_pos++];
    }

    if (p->read_pos >= p->write_pos) {
      p->read_pos = 0;
      p->write_pos = 0;
    }

    return n;
  }

  fs::vfs::Node *node = entry->node;

  if (node) {
    if (node->dev && node->dev->read)
      return node->dev->read(buf, len, entry->offset);

    if (!node->directory) {
      if (node->file && node->file->read) {
        node->file->offset = entry->offset;

        int n = node->file->read(node->file, (uint8_t *)buf, len);

        entry->offset = node->file->offset;

        return n;
      }

      if (!node->file || !node->file->private_data || node->file->size == 0) {
        return 0;
      }

      size_t size = node->file->size;

      if (entry->offset >= size)
        return 0;

      size_t remaining = size - entry->offset;
      size_t n = len < remaining ? len : remaining;

      memcpy(buf, (const char *)node->file->private_data + entry->offset, n);

      entry->offset += n;

      return n;
    }
  }

  return (uint64_t)-1;
}

static uint64_t sys_open(uint64_t a, uint64_t b, uint64_t c) {
  const char *path = (const char *)a;
  int flags = (int)b;
  int mode = (int)c;
  (void)mode;
  if (!path)
    return -1;

  char abs_path[256];
  if (path[0] != '/') {
    sched::Task *task = sched::get_current();
    const char *cwd = (task && task->process && task->process->cwd) ? fs::vfs::get_path(task->process->cwd) : "/";
    size_t cwd_len = strlen(cwd);
    if (cwd_len > 0 && cwd[cwd_len - 1] == '/') {
      ksnprintf(abs_path, sizeof(abs_path), "%s%s", cwd, path);
    } else {
      ksnprintf(abs_path, sizeof(abs_path), "%s/%s", cwd, path);
    }
    path = abs_path;
  }

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

  if (!node && (flags & 0x40)) {
    node = fs::vfs::create_file_path(path);
  }

  if (!node)
    return -1;

  int fd = alloc_fd(node);
  if (fd >= 0 && (flags & 0x200)) {
    fs::vfs::write_content(path, "", 0);
  }

  return fd;
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

static uint64_t sys_fcntl(uint64_t a, uint64_t b, uint64_t c) {
  int fd = (int)a;
  int cmd = (int)b;
  uint64_t arg = c;

  FdEntry *entry = get_fd_entry(fd);
  if (!entry || entry->type == FD_NONE)
    return (uint64_t)-1;

  switch (cmd) {
  case 0: { // F_DUPFD
    int min_fd = (int)arg;
    if (min_fd < 0 || min_fd >= MAX_FDS)
      return (uint64_t)-1;
    for (int i = min_fd; i < MAX_FDS; i++) {
      FdEntry *e = get_fd_entry(i);
      if (e && e->type == FD_NONE) {
        *e = *entry;
        return i;
      }
    }
    return (uint64_t)-1;
  }
  case 1: // F_GETFD
    return entry->flags;
  case 2: // F_SETFD
    entry->flags = (int)arg;
    return 0;
  case 3: // F_GETFL
    return entry->nonblock ? 0x800 : 0;
  case 4: // F_SETFL
    entry->nonblock = (arg & 0x800) != 0;
    return 0;
  case 1030: { // F_DUPFD_CLOEXEC
    int min_fd = (int)arg;
    if (min_fd < 0 || min_fd >= MAX_FDS)
      return (uint64_t)-1;
    for (int i = min_fd; i < MAX_FDS; i++) {
      FdEntry *e = get_fd_entry(i);
      if (e && e->type == FD_NONE) {
        *e = *entry;
        return i;
      }
    }
    return (uint64_t)-1;
  }
  default:
    return 0;
  }
}

static void sys_close_all(sched::Process *proc) {
  if (!proc)
    return;
  for (int i = 0; i < MAX_FDS; i++) {
    if (proc->fds[i].type != FD_NONE) {
      if (proc->fds[i].type == FD_PIPE && proc->fds[i].pipe) {
        if (proc->fds[i].readable)
          proc->fds[i].pipe->readers--;
        if (proc->fds[i].writable)
          proc->fds[i].pipe->writers--;
        if (proc->fds[i].pipe->readers <= 0 && proc->fds[i].pipe->writers <= 0) {
          for (int p = 0; p < 32; p++) {
            if (&pipes[p] == proc->fds[i].pipe)
              pipe_used[p] = false;
          }
        }
      } else if (proc->fds[i].mount && proc->fds[i].fs_handle && proc->fds[i].mount->fs && proc->fds[i].mount->fs->close) {
        proc->fds[i].mount->fs->close(proc->fds[i].fs_handle);
      }
      memset(&proc->fds[i], 0, sizeof(FdEntry));
    }
  }
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

  // Handle directory handles opened via filesystem mount (e.g. procfs subdirectories like /proc/1)
  if (!entry->node && entry->mount && entry->fs_handle && entry->mount->fs) {
    fs::procfs::ProcFile *pf = (fs::procfs::ProcFile *)entry->fs_handle;
    if (pf && pf->data) {
      size_t line_index = 0;
      const char *p = pf->data;
      while (*p) {
        const char *line_start = p;
        while (*p && *p != '\n')
          p++;
        size_t line_len = p - line_start;
        if (*p == '\n')
          p++;

        if (line_len > 0) {
          if (line_index == entry->offset) {
            memset(out, 0, sizeof(*out));
            out->d_ino = 0;
            out->d_off = entry->offset + 1;
            out->d_reclen = sizeof(*out);

            size_t copy_len = line_len < sizeof(out->d_name) - 1 ? line_len : sizeof(out->d_name) - 1;
            memcpy(out->d_name, line_start, copy_len);
            out->d_name[copy_len] = '\0';

            if (strcmp(out->d_name, "fd") == 0 || strcmp(out->d_name, "fdinfo") == 0 ||
                strcmp(out->d_name, "task") == 0 || strcmp(out->d_name, "cwd") == 0 ||
                strcmp(out->d_name, "exe") == 0 || strcmp(out->d_name, "root") == 0) {
              out->d_type = (strcmp(out->d_name, "cwd") == 0 || strcmp(out->d_name, "exe") == 0 ||
                             strcmp(out->d_name, "root") == 0) ? fs::vfs::DT_LNK : fs::vfs::DT_DIR;
            } else {
              out->d_type = fs::vfs::DT_REG;
            }

            entry->offset++;
            return 1;
          }
          line_index++;
        }
      }
      return 0;
    }
  }

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

  // Include active process PIDs when reading /proc directory
  if (dir->filesystem == &fs::procfs::filesystem || (dir->name && strcmp(dir->name, "proc") == 0)) {
    for (sched::Task *t = sched::head; t; t = t->next) {
      if (index == entry->offset) {
        memset(out, 0, sizeof(*out));

        out->d_ino = t->pid;
        out->d_off = entry->offset + 1;

        out->d_reclen = sizeof(*out);

        out->d_type = fs::vfs::DT_DIR;

        ksnprintf(out->d_name, sizeof(out->d_name), "%lu", t->pid);

        entry->offset++;

        return 1;
      }

      index++;
    }
  }

  return 0;
}

static uint64_t sys_ioctl(uint64_t a, uint64_t b, uint64_t c) {
  int fd = a;
  unsigned long req = b;
  void *arg = (void *)c;

  if (fd == 0 || fd == 1 || fd == 2) {
    return tty::ioctl(req, arg);
  }

  FdEntry *entry = get_fd_entry(fd);
  if (entry && entry->node && entry->node->dev && entry->node->dev->ioctl) {
    return entry->node->dev->ioctl(req, arg);
  }

  return tty::ioctl(req, arg);
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
    return sys_open(a, b, c);

  case SYS_CLOSE:
    return sys_close(a);

  case SYS_EXECVE:
    return sys_execve(a, b, c);

  case SYS_FORK:
    return sys_fork();

  case SYS_EXIT:
    if (sched::get_current())
      sys_close_all(sched::get_current()->process);
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

  case SYS_GETCWD: {
    char *buf = (char *)a;
    size_t size = b;

    if (!buf || size == 0)
      return -1;

    sched::Process *p = sched::get_current()->process;

    if (!p || !p->cwd)
      return -1;

    const char *path = fs::vfs::get_path(p->cwd);

    if (!path)
      return -1;

    strncpy(buf, path, size - 1);
    buf[size - 1] = '\0';

    return 0;
  }

  case SYS_CHDIR: {
    const char *path = (char *)a;

    auto p = sched::get_current()->process;

    auto node = fs::vfs::find(path);

    if (!node)
      return -1;

    if (!node->directory)
      return -1;

    p->cwd = node;

    return 0;
  }

  case SYS_UNLINK:
  case SYS_RMDIR: {
    const char *path = (const char *)a;
    if (!path)
      return -1;
    char abs_path[256];
    if (path[0] != '/') {
      sched::Task *task = sched::get_current();
      const char *cwd = (task && task->process && task->process->cwd) ? fs::vfs::get_path(task->process->cwd) : "/";
      size_t cwd_len = strlen(cwd);
      if (cwd_len > 0 && cwd[cwd_len - 1] == '/') {
        ksnprintf(abs_path, sizeof(abs_path), "%s%s", cwd, path);
      } else {
        ksnprintf(abs_path, sizeof(abs_path), "%s/%s", cwd, path);
      }
      path = abs_path;
    }
    return fs::vfs::remove(path) ? 0 : -1;
  }

  case SYS_MKDIR: {
    const char *path = (const char *)a;
    if (!path)
      return -1;
    char abs_path[256];
    if (path[0] != '/') {
      sched::Task *task = sched::get_current();
      const char *cwd = (task && task->process && task->process->cwd) ? fs::vfs::get_path(task->process->cwd) : "/";
      size_t cwd_len = strlen(cwd);
      if (cwd_len > 0 && cwd[cwd_len - 1] == '/') {
        ksnprintf(abs_path, sizeof(abs_path), "%s%s", cwd, path);
      } else {
        ksnprintf(abs_path, sizeof(abs_path), "%s/%s", cwd, path);
      }
      path = abs_path;
    }
    return fs::vfs::create_dir_path(path) ? 0 : -1;
  }

  case SYS_FCNTL:
    return sys_fcntl(a, b, c);

  default:
    kprintf("syscall: unknown syscall %lu\n", num);
    return (uint64_t)-1;
  }
}
