#include "LTOS/syscall.hpp"
#include "LTOS/drivers/console.hpp"
#include "LTOS/drivers/framebuffer.hpp"
#include "LTOS/drivers/mouse.hpp"
#include "LTOS/drivers/serial.hpp"
#include "LTOS/drivers/timer.hpp"
#include "LTOS/drivers/tty.hpp"
#include "LTOS/drivers/tty/ioctl.hpp"
#include "LTOS/fs/devfs.hpp"
#include "LTOS/fs/procfs.hpp"
#include "LTOS/fs/vfs.hpp"

#include "LTOS/lib/kprintf.h"
#include "LTOS/mm/heap.hpp"
#include "LTOS/mm/paging.hpp"
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

    if (pipe_used[i])
      continue;

    pipe_used[i] = true;

    Pipe *p = &pipes[i];
    memset(p, 0, sizeof(Pipe));

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

    if (rfd < 0 || wfd < 0) {
      pipe_used[i] = false;
      return -1;
    }

    p->readers = 1;
    p->writers = 1;

    memset(&proc->fds[rfd], 0, sizeof(FdEntry));
    memset(&proc->fds[wfd], 0, sizeof(FdEntry));

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

  return -1;
}

static uint64_t sys_write(uint64_t a, uint64_t b, uint64_t c) {
  int fd = (int)a;
  const char *buf = (const char *)b;
  size_t len = c;

  if (!buf || len == 0)
    return 0;

  FdEntry *entry = get_fd_entry(fd);

  if ((fd == 1 || fd == 2) &&
      (!entry || entry->type == sched::FD_NONE || entry->type == sched::FD_TTY))
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

  if (!entry->node && entry->mount && entry->fs_handle && entry->mount->fs &&
      entry->mount->fs->write) {
    return entry->mount->fs->write(entry->fs_handle, buf, len);
  }

  fs::vfs::Node *node = entry->node;

  if (node) {
    if (node->dev && node->dev->write) {
      size_t written = node->dev->write(buf, len, entry->offset);
      if (written > 0)
        entry->offset += written;
      return written;
    }

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
    if (node->dev && node->dev->read) {
      size_t read_bytes = node->dev->read(buf, len, entry->offset);
      if (read_bytes > 0)
        entry->offset += read_bytes;
      return read_bytes;
    }

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
    const char *cwd =
        (task && task->process && task->process->cwd) ? fs::vfs::get_path(task->process->cwd) : "/";
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
      } else if (proc->fds[i].mount && proc->fds[i].fs_handle && proc->fds[i].mount->fs &&
                 proc->fds[i].mount->fs->close) {
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

static uint64_t sys_nanosleep(uint64_t a, uint64_t b) {
  (void)b;
  struct timespec_k {
    int64_t tv_sec;
    int64_t tv_nsec;
  };
  const timespec_k *req = (const timespec_k *)a;
  if (!req)
    return (uint64_t)-1;

  uint64_t ms = (uint64_t)req->tv_sec * 1000 + (uint64_t)(req->tv_nsec / 1000000);
  if (ms == 0 && req->tv_nsec > 0)
    ms = 1;

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

  if (!entry->node && entry->mount && entry->fs_handle && entry->mount->fs &&
      entry->mount->fs->lseek) {
    long ret = entry->mount->fs->lseek(entry->fs_handle, offset, whence);
    if (ret >= 0)
      entry->offset = (size_t)ret;
    return ret;
  }

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

static void kill_task_tree(sched::Task *target) {
  if (!target || target->pid == 0 || target->pid == 1)
    return;

  sched::Task *t = sched::head;
  while (t) {
    sched::Task *next = t->next;
    if (t != target && (t->parent == target || (t->parent && t->parent->pid == target->pid))) {
      kill_task_tree(t);
    }
    t = next;
  }

  if (target->process) {
    sys_close_all(target->process);
  }

  target->state = sched::State::DEAD;
  sched::remove_and_destroy(target);
}

static uint64_t sys_kill(uint64_t a, uint64_t b) {
  uint64_t pid = a;
  int sig = (int)b;
  (void)sig;

  if (pid == 0 || pid == 1)
    return -1;

  sched::Task *cur = sched::get_current();
  if (cur && cur->pid == pid) {
    sched::exit(sig ? sig : 9);
    return 0;
  }

  sched::Task *t = sched::head;
  while (t) {
    if (t->pid == pid) {
      kill_task_tree(t);
      return 0;
    }
    t = t->next;
  }

  return -1;
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

    if (old_entry->type == FD_PIPE && old_entry->pipe) {
      if (old_entry->readable)
        old_entry->pipe->readers++;

      if (old_entry->writable)
        old_entry->pipe->writers++;
    }
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

            size_t copy_len =
                line_len < sizeof(out->d_name) - 1 ? line_len : sizeof(out->d_name) - 1;
            memcpy(out->d_name, line_start, copy_len);
            out->d_name[copy_len] = '\0';

            if (strcmp(out->d_name, "fd") == 0 || strcmp(out->d_name, "fdinfo") == 0 ||
                strcmp(out->d_name, "task") == 0 || strcmp(out->d_name, "cwd") == 0 ||
                strcmp(out->d_name, "exe") == 0 || strcmp(out->d_name, "root") == 0) {
              out->d_type = (strcmp(out->d_name, "cwd") == 0 || strcmp(out->d_name, "exe") == 0 ||
                             strcmp(out->d_name, "root") == 0)
                                ? fs::vfs::DT_LNK
                                : fs::vfs::DT_DIR;
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
  if (entry && !entry->node && entry->mount && entry->fs_handle && entry->mount->fs &&
      entry->mount->fs->ioctl) {
    return entry->mount->fs->ioctl(entry->fs_handle, req, arg);
  }

  if (entry && entry->node && entry->node->dev && entry->node->dev->ioctl) {
    return entry->node->dev->ioctl(req, arg);
  }

  // Previously fell through to tty::ioctl() here even when entry->node->dev
  // was a real device (e.g. an AHCI disk) that simply had no ioctl handler
  // wired up (ahci.cpp set dev_ops[ID].ioctl = nullptr). That meant a
  // program querying disk geometry (BLKGETSIZE64/BLKSSZGET-style calls
  // before partitioning) silently got a terminal-ioctl response instead
  // of an error -- causing tools like fdisk to read garbage geometry and
  // fail. A device that exists but genuinely doesn't support ioctl should
  // report that, not be treated as a tty.
  if (entry && entry->node && entry->node->dev)
    return (uint64_t)-1;

  return tty::ioctl(req, arg);
}

extern volatile size_t stdin_len;

struct pollfd_kernel {
  int fd;
  short events;
  short revents;
};

#ifndef POLLIN
#define POLLIN 0x0001
#define POLLOUT 0x0004
#define POLLERR 0x0008
#define POLLHUP 0x0010
#define POLLNVAL 0x0020
#endif

static uint64_t sys_poll(uint64_t a, uint64_t b, uint64_t c) {
  auto *pfds = (pollfd_kernel *)a;
  size_t nfds = b;
  int timeout_ms = (int)c;

  if (!pfds || nfds == 0)
    return 0;

  uint64_t start_ms = timer::ticks();

  while (true) {
    int ready = 0;

    for (size_t i = 0; i < nfds; i++) {
      pfds[i].revents = 0;
      int fd = pfds[i].fd;

      if (fd == 0 || fd == 1 || fd == 2) {
        if (fd == 0) {
          if (pfds[i].events & POLLIN) {
            if (stdin_len > 0) {
              pfds[i].revents |= POLLIN;
              ready++;
            }
          }
        }
        if (fd == 1 || fd == 2) {
          if (pfds[i].events & POLLOUT) {
            pfds[i].revents |= POLLOUT;
            ready++;
          }
        }
        continue;
      }

      FdEntry *entry = get_fd_entry(fd);
      if (!entry || entry->type == FD_NONE) {
        pfds[i].revents |= POLLNVAL;
        ready++;
        continue;
      }

      if (entry->type == FD_PIPE) {
        Pipe *p = entry->pipe;
        if (entry->readable) {
          if (p->read_pos < p->write_pos) {
            if (pfds[i].events & POLLIN) {
              pfds[i].revents |= POLLIN;
              ready++;
            }
          } else if (p->writers <= 0) {
            pfds[i].revents |= POLLHUP;
            ready++;
          }
        }
        if (entry->writable) {
          if (p->write_pos < sizeof(p->buffer)) {
            if (pfds[i].events & POLLOUT) {
              pfds[i].revents |= POLLOUT;
              ready++;
            }
          }
        }
        continue;
      }

      if (pfds[i].events & POLLIN)
        pfds[i].revents |= POLLIN;
      if (pfds[i].events & POLLOUT)
        pfds[i].revents |= POLLOUT;
      ready++;
    }

    if (ready > 0)
      return ready;

    if (timeout_ms == 0)
      return 0;

    if (timeout_ms > 0) {
      uint64_t elapsed = timer::ticks() - start_ms;
      if (elapsed >= (uint64_t)timeout_ms) {
        return 0;
      }
    }

    timer::sleep(1);
  }
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

static uint64_t sys_mount(uint64_t source_ptr, uint64_t target_ptr, uint64_t fstype_ptr,
                          uint64_t flags, uint64_t data_ptr) {
  const char *target = (const char *)target_ptr;
  const char *fstype = (const char *)fstype_ptr;

  if (!target || !target[0])
    return -1;

  char abs_target[256];
  if (target[0] != '/') {
    sched::Task *task = sched::get_current();
    const char *cwd =
        (task && task->process && task->process->cwd) ? fs::vfs::get_path(task->process->cwd) : "/";
    size_t cwd_len = strlen(cwd);
    if (cwd_len > 0 && cwd[cwd_len - 1] == '/') {
      ksnprintf(abs_target, sizeof(abs_target), "%s%s", cwd, target);
    } else {
      ksnprintf(abs_target, sizeof(abs_target), "%s/%s", cwd, target);
    }
    target = abs_target;
  }

  fs::FileSystem *fs = nullptr;
  if (fstype) {
    fs = fs::get_filesystem(fstype);
  }

  static fs::FileSystem tmpfs_fs = {
      .name = "tmpfs",
      .init = nullptr,
      .open = nullptr,
      .read = nullptr,
      .write = nullptr,
      .close = nullptr,
      .list = nullptr,
      .ioctl = nullptr,
  };

  if (!fs && fstype) {
    if (strcmp(fstype, "tmpfs") == 0 || strcmp(fstype, "ramfs") == 0) {
      fs = &tmpfs_fs;
    }
  }

  return fs::mount(target, fs, (void *)data_ptr) ? 0 : -1;
}

static uint64_t sys_umount(uint64_t target_ptr) {
  const char *target = (const char *)target_ptr;
  if (!target || !target[0])
    return -1;

  char abs_target[256];
  if (target[0] != '/') {
    sched::Task *task = sched::get_current();
    const char *cwd =
        (task && task->process && task->process->cwd) ? fs::vfs::get_path(task->process->cwd) : "/";
    size_t cwd_len = strlen(cwd);
    if (cwd_len > 0 && cwd[cwd_len - 1] == '/') {
      ksnprintf(abs_target, sizeof(abs_target), "%s%s", cwd, target);
    } else {
      ksnprintf(abs_target, sizeof(abs_target), "%s/%s", cwd, target);
    }
    target = abs_target;
  }

  return fs::umount(target) ? 0 : -1;
}

#include "LTOS/mm/pmm.hpp"

struct kernel_stat {
  uint64_t st_dev;
  uint64_t st_ino;
  uint64_t st_mode;
  uint64_t st_nlink;
  uint64_t st_uid;
  uint64_t st_gid;
  uint64_t st_size;
  uint64_t st_atime;
  uint64_t st_mtime;
  uint64_t st_ctime;
  uint64_t st_blksize;
  uint64_t st_blocks;
};

static uint64_t sys_stat(uint64_t path_ptr, uint64_t buf_ptr) {
  const char *path = (const char *)path_ptr;
  auto *st = (struct kernel_stat *)buf_ptr;
  if (!path || !st)
    return -1;

  char abs_path[256];
  if (path[0] != '/') {
    sched::Task *task = sched::get_current();
    const char *cwd =
        (task && task->process && task->process->cwd) ? fs::vfs::get_path(task->process->cwd) : "/";
    size_t cwd_len = strlen(cwd);
    if (cwd_len > 0 && cwd[cwd_len - 1] == '/') {
      ksnprintf(abs_path, sizeof(abs_path), "%s%s", cwd, path);
    } else {
      ksnprintf(abs_path, sizeof(abs_path), "%s/%s", cwd, path);
    }
    path = abs_path;
  }

  auto node = fs::vfs::find(path);
  if (!node)
    return -1;

  memset(st, 0, sizeof(struct kernel_stat));
  st->st_dev = 1;
  st->st_ino = (uint64_t)node;
  st->st_mode = node->directory ? (0040000 | 0755) : (0100000 | 0644);
  st->st_nlink = 1;
  st->st_uid = 0;
  st->st_gid = 0;
  st->st_size = node->file ? node->file->size : 0;
  st->st_blksize = 512;
  st->st_blocks = (st->st_size + 511) / 512;
  return 0;
}

static uint64_t sys_fstat(uint64_t fd, uint64_t buf_ptr) {
  auto *st = (struct kernel_stat *)buf_ptr;
  if (!st)
    return -1;

  FdEntry *entry = get_fd_entry((int)fd);
  if (!entry || entry->type == FD_NONE)
    return -1;

  auto node = entry->node;
  memset(st, 0, sizeof(struct kernel_stat));
  st->st_dev = 1;
  st->st_ino = node ? (uint64_t)node : (uint64_t)entry;
  st->st_mode = (node && node->directory) ? (0040000 | 0755) : (0100000 | 0644);
  st->st_nlink = 1;
  st->st_uid = 0;
  st->st_gid = 0;
  st->st_size = node && node->file ? node->file->size : 0;
  st->st_blksize = 512;
  st->st_blocks = (st->st_size + 511) / 512;
  return 0;
}

static uint64_t sys_dup(uint64_t oldfd) {
  sched::Task *task = sched::get_current();
  if (!task || !task->process)
    return -1;
  FdEntry *old_entry = get_fd_entry((int)oldfd);
  if (!old_entry || old_entry->type == FD_NONE)
    return -1;

  for (int i = 0; i < MAX_FDS; i++) {
    if (task->process->fds[i].type == FD_NONE) {
      task->process->fds[i] = *old_entry;

      if (old_entry->type == FD_PIPE && old_entry->pipe) {
        if (old_entry->readable)
          old_entry->pipe->readers++;

        if (old_entry->writable)
          old_entry->pipe->writers++;
      }

      return i;
    }
  }
  return -1;
}

static uint64_t sys_access(uint64_t path_ptr, uint64_t mode) {
  const char *path = (const char *)path_ptr;
  if (!path)
    return -1;

  char abs_path[256];
  if (path[0] != '/') {
    sched::Task *task = sched::get_current();
    const char *cwd =
        (task && task->process && task->process->cwd) ? fs::vfs::get_path(task->process->cwd) : "/";
    size_t cwd_len = strlen(cwd);
    if (cwd_len > 0 && cwd[cwd_len - 1] == '/') {
      ksnprintf(abs_path, sizeof(abs_path), "%s%s", cwd, path);
    } else {
      ksnprintf(abs_path, sizeof(abs_path), "%s/%s", cwd, path);
    }
    path = abs_path;
  }

  auto node = fs::vfs::find(path);
  return node ? 0 : -1;
}

static uint64_t sys_uname(uint64_t buf_ptr) {
  struct utsname {
    char sysname[65];
    char nodename[65];
    char release[65];
    char version[65];
    char machine[65];
    char domainname[65];
  } *u = (struct utsname *)buf_ptr;

  if (!u)
    return -1;
  strncpy(u->sysname, "LosTacOS", 64);
  strncpy(u->nodename, "lostacos", 64);
  strncpy(u->release, "0.4.0", 64);
  strncpy(u->version, "LosTacOS 0.4.0 x86_64", 64);
  strncpy(u->machine, "x86_64", 64);
  strncpy(u->domainname, "(none)", 64);
  return 0;
}

static uint64_t sys_sysinfo(uint64_t buf_ptr) {
  struct sysinfo {
    long uptime;
    unsigned long loads[3];
    unsigned long totalram;
    unsigned long freeram;
    unsigned long sharedram;
    unsigned long bufferram;
    unsigned long totalswap;
    unsigned long freeswap;
    unsigned short procs;
    unsigned short pad;
    unsigned long totalhigh;
    unsigned long freehigh;
    unsigned int mem_unit;
    char _f[20 - 2 * sizeof(long) - sizeof(int)];
  } *info = (struct sysinfo *)buf_ptr;

  if (!info)
    return -1;
  memset(info, 0, sizeof(struct sysinfo));
  info->uptime = timer::ticks() / 1000;
  info->totalram = pmm::total_memory();
  info->freeram = pmm::free_memory();
  info->procs = 1;
  info->mem_unit = 1;
  return 0;
}

static uint64_t sys_getdents64(uint64_t fd, uint64_t dirp_ptr, uint64_t count) {
  FdEntry *entry = get_fd_entry((int)fd);
  if (!entry || entry->type == FD_NONE)
    return -1;

  auto node = entry->node;
  if (!node || !node->directory)
    return -1;

  char *buf = (char *)dirp_ptr;
  if (!buf || count == 0)
    return 0;

  size_t offset = 0;
  int d_ino = 1;

  for (auto child = node->children; child; child = child->next) {
    size_t name_len = strlen(child->name);
    size_t rec_len =
        (sizeof(uint64_t) * 2 + sizeof(unsigned short) + sizeof(unsigned char) + name_len + 1 + 7) &
        ~7;
    if (offset + rec_len > count)
      break;

    struct linux_dirent64 {
      uint64_t d_ino;
      int64_t d_off;
      unsigned short d_reclen;
      unsigned char d_type;
      char d_name[];
    } *dent = (struct linux_dirent64 *)(buf + offset);

    dent->d_ino = d_ino++;
    dent->d_off = offset + rec_len;
    dent->d_reclen = rec_len;
    dent->d_type = child->directory ? 4 : 8;
    strcpy(dent->d_name, child->name);

    offset += rec_len;
  }

  return offset;
}

static uint64_t sys_link(uint64_t oldpath_ptr, uint64_t newpath_ptr) {
  const char *oldpath = (const char *)oldpath_ptr;
  const char *newpath = (const char *)newpath_ptr;
  if (!oldpath || !newpath)
    return -1;

  char abs_old[256], abs_new[256];
  if (oldpath[0] != '/') {
    sched::Task *task = sched::get_current();
    const char *cwd =
        (task && task->process && task->process->cwd) ? fs::vfs::get_path(task->process->cwd) : "/";
    size_t cwd_len = strlen(cwd);
    if (cwd_len > 0 && cwd[cwd_len - 1] == '/')
      ksnprintf(abs_old, sizeof(abs_old), "%s%s", cwd, oldpath);
    else
      ksnprintf(abs_old, sizeof(abs_old), "%s/%s", cwd, oldpath);
    oldpath = abs_old;
  }
  if (newpath[0] != '/') {
    sched::Task *task = sched::get_current();
    const char *cwd =
        (task && task->process && task->process->cwd) ? fs::vfs::get_path(task->process->cwd) : "/";
    size_t cwd_len = strlen(cwd);
    if (cwd_len > 0 && cwd[cwd_len - 1] == '/')
      ksnprintf(abs_new, sizeof(abs_new), "%s%s", cwd, newpath);
    else
      ksnprintf(abs_new, sizeof(abs_new), "%s/%s", cwd, newpath);
    newpath = abs_new;
  }

  auto node = fs::vfs::find(oldpath);
  if (!node)
    return -1;

  return fs::vfs::create_symlink_path(newpath, oldpath) ? 0 : -1;
}

static uint64_t sys_symlink(uint64_t target_ptr, uint64_t linkpath_ptr) {
  const char *target = (const char *)target_ptr;
  const char *linkpath = (const char *)linkpath_ptr;
  if (!target || !linkpath)
    return -1;

  char abs_link[256];
  if (linkpath[0] != '/') {
    sched::Task *task = sched::get_current();
    const char *cwd =
        (task && task->process && task->process->cwd) ? fs::vfs::get_path(task->process->cwd) : "/";
    size_t cwd_len = strlen(cwd);
    if (cwd_len > 0 && cwd[cwd_len - 1] == '/')
      ksnprintf(abs_link, sizeof(abs_link), "%s%s", cwd, linkpath);
    else
      ksnprintf(abs_link, sizeof(abs_link), "%s/%s", cwd, linkpath);
    linkpath = abs_link;
  }

  return fs::vfs::create_symlink_path(linkpath, target) ? 0 : -1;
}

static uint64_t sys_readlink(uint64_t path_ptr, uint64_t buf_ptr, uint64_t bufsize) {
  const char *path = (const char *)path_ptr;
  char *buf = (char *)buf_ptr;
  if (!path || !buf || bufsize == 0)
    return -1;

  char abs_path[256];
  if (path[0] != '/') {
    sched::Task *task = sched::get_current();
    const char *cwd =
        (task && task->process && task->process->cwd) ? fs::vfs::get_path(task->process->cwd) : "/";
    size_t cwd_len = strlen(cwd);
    if (cwd_len > 0 && cwd[cwd_len - 1] == '/')
      ksnprintf(abs_path, sizeof(abs_path), "%s%s", cwd, path);
    else
      ksnprintf(abs_path, sizeof(abs_path), "%s/%s", cwd, path);
    path = abs_path;
  }

  auto node = fs::vfs::find(path);
  if (!node || !node->symlink)
    return -1;

  size_t len = strlen(node->symlink);
  if (len > bufsize)
    len = bufsize;
  memcpy(buf, node->symlink, len);
  return len;
}

extern "C" uint64_t syscall_handler(uint64_t num, uint64_t a, uint64_t b, uint64_t c, uint64_t d,
                                    uint64_t e, uint64_t f) {
  // kprintf("SYSCALL %lu a=%lx b=%lx c=%lx\n", num, a, b, c);

  switch (num) {

  case SYS_STAT:
  case SYS_LSTAT:
    return sys_stat(a, b);

  case SYS_NEWFSTATAT:
    return sys_stat(b, c);

  case SYS_FSTAT:
    return sys_fstat(a, b);

  case SYS_DUP:
    return sys_dup(a);

  case SYS_ACCESS:
    return sys_access(a, b);

  case SYS_UNAME:
    return sys_uname(a);

  case SYS_SYSINFO:
    return sys_sysinfo(a);

  case SYS_GETDENTS:
  case SYS_GETDENTS64:
    return sys_getdents64(a, b, c);

  case SYS_GETUID:
  case SYS_GETGID:
  case SYS_GETEUID:
  case SYS_GETEGID:
  case SYS_SETUID:
  case SYS_SETGID:
  case SYS_MPROTECT:
  case SYS_BRK:
  case SYS_RT_SIGACTION:
  case SYS_RT_SIGPROCMASK:
  case SYS_PAUSE:
  case SYS_GETITIMER:
  case SYS_ALARM:
  case SYS_SETITIMER:
  case SYS_TRUNCATE:
  case SYS_FTRUNCATE:
  case SYS_CHMOD:
  case SYS_FCHMOD:
  case SYS_CHOWN:
  case SYS_FCHOWN:
  case SYS_LCHOWN:
  case SYS_FUTEX:
    return 0;

  case SYS_UMASK:
    return 022;

  case SYS_GETPPID: {
    sched::Task *task = sched::get_current();
    return (task && task->parent && task->parent->process) ? task->parent->process->pid : 1;
  }

  case SYS_MOUNT:
    return sys_mount(a, b, c, d, e);

  case SYS_UMOUNT2:
    return sys_umount(a);

  case SYS_POLL:
    return sys_poll(a, b, c);

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
  case SYS_VFORK:
  case SYS_CLONE:
    return sys_fork();

  case SYS_EXIT:
    if (sched::get_current())
      sys_close_all(sched::get_current()->process);
    sched::exit(a);
    return 0;

  case SYS_GETPID:
    return sys_getpid();

  case SYS_LSEEK:
    return sys_lseek(a, b, c);

  case SYS_WAIT4:
    return sys_wait(a);

  case SYS_KILL:
    return sys_kill(a, b);

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

  case SYS_GETTIMEOFDAY:
    return sys_gettimeofday(a);

  case SYS_LINK:
    return sys_link(a, b);

  case SYS_SYMLINK:
    return sys_symlink(a, b);

  case SYS_READLINK:
  case SYS_READLINKAT:
    return sys_readlink(a, b, c);

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
  case SYS_RMDIR:
    return fs::vfs::remove((const char *)a) ? 0 : -1;

  case SYS_UNLINKAT:
    return fs::vfs::remove((const char *)b) ? 0 : -1;

  case SYS_MKDIR:
    return fs::vfs::create_dir_path((const char *)a) ? 0 : -1;

  case SYS_MKDIRAT:
    return fs::vfs::create_dir_path((const char *)b) ? 0 : -1;

  case SYS_EXIT_GROUP:
    if (sched::get_current())
      sys_close_all(sched::get_current()->process);
    sched::exit(a);
    return 0;

  case SYS_OPENAT:
    return sys_open(b, c, d);

  case SYS_TIME: {
    uint64_t sec = timer::ticks() / 1000;
    if (a)
      *(uint64_t *)a = sec;
    return sec;
  }

  case SYS_SCHED_YIELD:
  case SYS_YIELD:
    return sys_yield();

  case SYS_SLEEP:
    return sys_sleep(a);

  case SYS_NANOSLEEP:
    return sys_nanosleep(a, b);

  case SYS_READDIR:
    return sys_readdir(a, b);

  case SYS_FSIZE:
    return sys_fsize(a);

  case SYS_SET_TID_ADDRESS:
    return sys_getpid();

  case SYS_PIPE2:
    return sys_pipe(a);

  case SYS_FCNTL:
    return sys_fcntl(a, b, c);

  default:
    kprintf("syscall: unknown syscall %lu\n", num);
    return (uint64_t)-1;
  }
}
