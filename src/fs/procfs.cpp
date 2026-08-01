#include "LTOS/fs/procfs.hpp"
#include "LTOS/arch/x86_64/cpu.hpp"
#include "LTOS/arch/x86_64/paging.hpp"
#include "LTOS/drivers/timer.hpp"
#include "LTOS/fs/fs.hpp"
#include "LTOS/fs/vfs.hpp"
#include "LTOS/lib/kprintf.h"
#include "LTOS/mm/heap.hpp"
#include "LTOS/mm/pmm.hpp"
#include "LTOS/sched/process.hpp"
#include "LTOS/sched/scheduler.hpp"
#include "LTOS_gen/version.h"

#include <string.h>

// Yes i know some of the values are placeholders now fuck off

namespace multiboot2 {
extern char boot_cmdline[256];
}

namespace fs::procfs {

struct ProcEntry {
  const char *name;
  const char *(*generate)();
};

static char buffer[2048];

static uint64_t count_user_pages(mm::AddressSpace *space) {
  if (!space || !space->table || !space->table->pml4)
    return 0;

  uint64_t *pml4 = space->table->pml4;
  if (!(pml4[0] & PAGE_PRESENT))
    return 0;

  uint64_t *pdpt = (uint64_t *)(pml4[0] & ~0xFFFULL);
  if (!pdpt)
    return 0;

  uint64_t pages = 0;

  for (int pi = 0; pi < 512; pi++) {
    if (!(pdpt[pi] & PAGE_PRESENT))
      continue;

    uint64_t *pd = (uint64_t *)(pdpt[pi] & ~0xFFFULL);
    for (int di = 0; di < 512; di++) {
      if (!(pd[di] & PAGE_PRESENT))
        continue;

      uint64_t *pt = (uint64_t *)(pd[di] & ~0xFFFULL);
      for (int ti = 0; ti < 512; ti++) {
        if (pt[ti] & PAGE_PRESENT) {
          pages++;
        }
      }
    }
  }

  return pages;
}

static const char *get_version() {
  ksnprintf(buffer, sizeof(buffer), "LosTacOS v%s\n", LTOS_VERSION);
  return buffer;
}

static const char *get_uptime() {
  uint64_t ms = timer::get_uptime_ms();
  uint64_t s = ms / 1000;
  ksnprintf(buffer, sizeof(buffer), "%lu.%02lu %lu.00\n", s, (ms % 1000) / 10, s);
  return buffer;
}

static const char *get_meminfo() {
  uint64_t total_kb = pmm::total_memory() / 1024;
  uint64_t free_kb = pmm::free_memory() / 1024;
  uint64_t used_kb = pmm::used_memory() / 1024;

  ksnprintf(buffer, sizeof(buffer),
            "MemTotal: %lu kB\n"
            "MemFree: %lu kB\n"
            "MemUsed: %lu kB\n",
            total_kb, free_kb, used_kb);

  return buffer;
}

static const char *get_cpuinfo() {
  size_t offset = 0;
  buffer[0] = 0;

  for (uint32_t i = 0; i < arch::cpu::g_smp_info.num_cpus; i++) {
    const auto &cpu = arch::cpu::g_smp_info.cpus[i];
    offset += ksnprintf(
        buffer + offset, sizeof(buffer) - offset,
        "processor\t: %u\n"
        "vendor_id\t: %s\n"
        "cpu family\t: %u\n"
        "model\t\t: %u\n"
        "model name\t: %s\n"
        "stepping\t: %u\n"
        "cpu MHz\t\t: 2400.000\n"
        "cache size\t: 512 KB\n"
        "physical id\t: 0\n"
        "siblings\t: %u\n"
        "core id\t\t: %u\n"
        "cpu cores\t: %u\n"
        "apicid\t\t: %u\n"
        "fpu\t\t: yes\n"
        "fpu_exception\t: yes\n"
        "flags\t\t: %s\n"
        "bogomips\t: 4800.00\n"
        "clflush size\t: 64\n"
        "cache_alignment\t: 64\n"
        "address sizes\t: 39 bits physical, 48 bits virtual\n\n",
        i, cpu.vendor[0] ? cpu.vendor : "Unknown", cpu.family, cpu.model,
        cpu.brand[0] ? cpu.brand : "x86_64 Processor", cpu.stepping, cpu.logical_cores, i,
        cpu.physical_cores, cpu.apic_id,
        cpu.flags_str[0]
            ? cpu.flags_str
            : "fpu vme de pse tsc msr pae mce cx8 apic sse sse2 sse3 sse4_1 sse4_2 avx avx2");
  }

  return buffer;
}

static const char *get_cmdline() {
  ksnprintf(buffer, sizeof(buffer), "%s\n", multiboot2::boot_cmdline);
  return buffer;
}

static const char *get_stat() {
  uint64_t ticks = timer::ticks();
  uint64_t running = 0;
  uint64_t blocked = 0;
  uint64_t total = 0;

  for (sched::Task *t = sched::head; t; t = t->next) {
    total++;
    if (t->state == sched::State::RUNNING || t->state == sched::State::READY)
      running++;
    else if (t->state == sched::State::BLOCKED)
      blocked++;
  }

  size_t offset =
      ksnprintf(buffer, sizeof(buffer), "cpu  %lu 0 %lu 0 0 0 0 0 0 0\n", ticks * 10, ticks);

  for (uint32_t i = 0; i < arch::cpu::g_smp_info.num_cpus; i++) {
    offset += ksnprintf(buffer + offset, sizeof(buffer) - offset, "cpu%u %lu 0 %lu 0 0 0 0 0 0 0\n",
                        i, (ticks * 10) / arch::cpu::g_smp_info.num_cpus,
                        ticks / arch::cpu::g_smp_info.num_cpus);
  }

  offset += ksnprintf(buffer + offset, sizeof(buffer) - offset,
                      "btime 1700000000\n"
                      "processes %lu\n"
                      "procs_running %lu\n"
                      "procs_blocked %lu\n",
                      total, running, blocked);
  return buffer;
}

static const char *get_mounts() {
  size_t offset = 0;
  buffer[0] = 0;

  for (fs::Mount *m = fs::get_mounts(); m; m = m->next) {
    const char *fs_name = (m->fs && m->fs->name) ? m->fs->name : "unknown";
    offset += ksnprintf(buffer + offset, sizeof(buffer) - offset, "%s %s %s rw 0 0\n", fs_name,
                        m->path, fs_name);
  }

  return buffer;
}

static const char *get_devices() {
  size_t offset = ksnprintf(buffer, sizeof(buffer), "Character devices:\n");
  vfs::Node *dev_dir = vfs::find("/dev");
  if (dev_dir && dev_dir->children) {
    vfs::Node *child = dev_dir->children;
    int minor = 1;
    while (child && offset < sizeof(buffer) - 32) {
      offset +=
          ksnprintf(buffer + offset, sizeof(buffer) - offset, "  %d %s\n", minor++, child->name);
      child = child->next;
    }
  }
  return buffer;
}

static const char *get_filesystems() {
  size_t offset = 0;
  buffer[0] = 0;
  for (fs::Mount *m = fs::get_mounts(); m; m = m->next) {
    const char *fs_name = (m->fs && m->fs->name) ? m->fs->name : "unknown";
    offset += ksnprintf(buffer + offset, sizeof(buffer) - offset, "nodev\t%s\n", fs_name);
  }
  return buffer;
}

static const char *get_interrupts() {
  uint64_t ticks = timer::ticks();
  ksnprintf(buffer, sizeof(buffer),
            "            CPU0\n"
            "  0:  %10lu   IO-APIC-edge      timer\n"
            "  1:         120   IO-APIC-edge      i8042\n"
            "  4:          45   IO-APIC-edge      serial\n",
            ticks);
  return buffer;
}

static const char *get_loadavg() {
  uint64_t total = 0;
  uint64_t running = 0;

  for (sched::Task *t = sched::head; t; t = t->next) {
    total++;
    if (t->state == sched::State::RUNNING || t->state == sched::State::READY)
      running++;
  }

  sched::Task *curr = sched::get_current();
  uint64_t curr_pid = curr ? curr->pid : 1;

  ksnprintf(buffer, sizeof(buffer), "%lu.%02lu 0.01 0.00 %lu/%lu %lu\n", running,
            (running * 10) % 100, running, total, curr_pid);
  return buffer;
}

static const char *get_diskstats() {
  uint64_t ticks = timer::ticks();
  ksnprintf(buffer, sizeof(buffer), "   8       0 sda %lu %lu %lu %lu %lu %lu %lu %lu 0 %lu %lu\n",
            ticks / 10, ticks / 20, ticks * 8, ticks, ticks / 30, ticks / 40, ticks * 2, ticks);
  return buffer;
}

// static const char *get_net_dev() {
//   uint64_t ticks = timer::ticks();
//   ksnprintf(buffer, sizeof(buffer),
//             "Inter-|   Receive                                                |  Transmit\n"
//             " face |bytes    packets errs drop fifo frame compressed multicast|bytes    packets "
//             "errs drop fifo colls carrier compressed\n"
//             "    lo: %7lu   %5lu    0    0    0     0          0         0  %7lu   %5lu    0    0
//             " "  0     0       0          0\n" "  eth0: %7lu   %5lu    0    0    0     0 0 0 %7lu
//             %5lu    0    0  " "  0     0       0          0\n", ticks * 128, ticks, ticks * 128,
//             ticks, ticks * 1024, ticks * 8, ticks * 512, ticks * 4);
//   return buffer;
// }

static const char *get_swaps() {
  return "Filename\t\t\t\tType\t\tSize\tUsed\tPriority\n";
}

static ProcEntry entries[] = {
    {"version", get_version},       {"uptime", get_uptime},   {"meminfo", get_meminfo},
    {"cpuinfo", get_cpuinfo},       {"cmdline", get_cmdline}, {"stat", get_stat},
    {"mounts", get_mounts},         {"devices", get_devices}, {"filesystems", get_filesystems},
    {"interrupts", get_interrupts}, {"loadavg", get_loadavg}, {"swaps", get_swaps},
    {"diskstats", get_diskstats},   {nullptr, nullptr}};

static ProcFile *make_proc_file(const char *content) {
  if (!content)
    return nullptr;

  ProcFile *file = (ProcFile *)heap::kmalloc(sizeof(ProcFile));
  if (!file)
    return nullptr;

  size_t len = strlen(content);
  file->data = (char *)heap::kmalloc(len + 1);
  if (!file->data) {
    heap::kfree(file);
    return nullptr;
  }

  memcpy(file->data, content, len);
  file->data[len] = 0;
  file->size = len;
  file->offset = 0;
  return file;
}

static void *open(void *, const char *path) {
  if (!path)
    return nullptr;

  // Handle /proc/self aliases
  if (strcmp(path, "self") == 0 || strcmp(path, "/self") == 0) {
    sched::Task *curr = sched::get_current();
    if (curr) {
      char pid_path[64];
      ksnprintf(pid_path, sizeof(pid_path), "%lu", curr->pid);
      return open(nullptr, pid_path);
    }
  }

  if (strncmp(path, "self/", 5) == 0 || strncmp(path, "/self/", 6) == 0) {
    sched::Task *curr = sched::get_current();
    if (curr) {
      char pid_path[64];
      const char *sub = (path[0] == '/') ? path + 6 : path + 5;
      ksnprintf(pid_path, sizeof(pid_path), "%lu/%s", curr->pid, sub);
      return open(nullptr, pid_path);
    }
  }

  // Check static entries (/proc/version, /proc/meminfo, etc.)
  for (int i = 0; entries[i].name; i++) {
    if (strcmp(path, entries[i].name) == 0) {
      return make_proc_file(entries[i].generate());
    }
  }

  // Dynamic PID support (/proc/<PID>/*)
  if (path[0] >= '0' && path[0] <= '9') {
    uint64_t pid = 0;
    const char *p = path;
    while (*p >= '0' && *p <= '9') {
      pid = pid * 10 + (*p - '0');
      p++;
    }

    sched::Task *t = sched::find(pid);
    if (t) {
      // Opening /proc/<PID> or /proc/<PID>/
      if (p[0] == 0 || (p[0] == '/' && p[1] == 0)) {
        ksnprintf(buffer, sizeof(buffer),
                  "cmdline\n"
                  "comm\n"
                  "cwd\n"
                  "environ\n"
                  "exe\n"
                  "fd\n"
                  "fdinfo\n"
                  "io\n"
                  "maps\n"
                  "mounts\n"
                  "mountinfo\n"
                  "root\n"
                  "stat\n"
                  "statm\n"
                  "status\n"
                  "task\n"
                  "wchan\n"
                  "cgroup\n");
        return make_proc_file(buffer);
      }

      if (strcmp(p, "/status") == 0 || strcmp(p, "status") == 0) {
        const char *state_str = (t->state == sched::State::RUNNING)   ? "R (running)"
                                : (t->state == sched::State::READY)   ? "R (ready)"
                                : (t->state == sched::State::BLOCKED) ? "S (sleeping)"
                                                                      : "Z (zombie)";

        const char *proc_name = (t->process && t->process->name[0]) ? t->process->name : "task";
        uint64_t parent_pid = (t->parent) ? t->parent->pid : 0;
        uint64_t user_pages = count_user_pages(t->process ? t->process->space : nullptr);
        uint64_t vmsize_kb = user_pages * 4;

        ksnprintf(buffer, sizeof(buffer),
                  "Name:\t%s\n"
                  "State:\t%s\n"
                  "Tgid:\t%lu\n"
                  "Pid:\t%lu\n"
                  "PPid:\t%lu\n"
                  "TracerPid:\t0\n"
                  "Uid:\t0\t0\t0\t0\n"
                  "Gid:\t0\t0\t0\t0\n"
                  "FDSize:\t32\n"
                  "VmSize:\t%lu kB\n"
                  "VmRSS:\t%lu kB\n"
                  "VmData:\t%lu kB\n"
                  "VmStk:\t16 kB\n"
                  "VmExe:\t4 kB\n"
                  "Threads:\t1\n",
                  proc_name, state_str, pid, pid, parent_pid, vmsize_kb, vmsize_kb, vmsize_kb);
        return make_proc_file(buffer);
      }

      if (strcmp(p, "/stat") == 0 || strcmp(p, "stat") == 0) {
        char state_char = (t->state == sched::State::RUNNING || t->state == sched::State::READY)
                              ? 'R'
                          : (t->state == sched::State::BLOCKED) ? 'S'
                                                                : 'Z';
        const char *proc_name = (t->process && t->process->name[0]) ? t->process->name : "task";
        uint64_t parent_pid = (t->parent) ? t->parent->pid : 0;
        uint64_t user_pages = count_user_pages(t->process ? t->process->space : nullptr);
        uint64_t vsize = user_pages * 4096;
        uint64_t rss = user_pages;

        ksnprintf(buffer, sizeof(buffer),
                  "%lu (%s) %c %lu %lu %lu 0 -1 0 0 0 0 0 %lu 0 0 0 20 0 1 0 0 %lu %lu "
                  "18446744073709551615 0 0 0 0 0 0 0 0 0 0 0 17 0 0\n",
                  pid, proc_name, state_char, parent_pid, pid, pid, timer::ticks(), vsize, rss);
        return make_proc_file(buffer);
      }

      if (strcmp(p, "/cmdline") == 0 || strcmp(p, "cmdline") == 0) {
        const char *proc_name = (t->process && t->process->name[0]) ? t->process->name : "task";
        ksnprintf(buffer, sizeof(buffer), "%s\n", proc_name);
        return make_proc_file(buffer);
      }

      if (strcmp(p, "/comm") == 0 || strcmp(p, "comm") == 0) {
        const char *proc_name = (t->process && t->process->name[0]) ? t->process->name : "task";
        const char *comm = proc_name;
        for (const char *c = proc_name; *c; c++) {
          if (*c == '/' && *(c + 1))
            comm = c + 1;
        }
        ksnprintf(buffer, sizeof(buffer), "%s\n", comm);
        return make_proc_file(buffer);
      }

      if (strcmp(p, "/cwd") == 0 || strcmp(p, "cwd") == 0) {
        const char *cwd_path =
            (t->process && t->process->cwd) ? vfs::get_path(t->process->cwd) : "/";
        ksnprintf(buffer, sizeof(buffer), "%s\n", cwd_path);
        return make_proc_file(buffer);
      }

      if (strcmp(p, "/exe") == 0 || strcmp(p, "exe") == 0) {
        const char *proc_name = (t->process && t->process->name[0]) ? t->process->name : "task";
        if (proc_name[0] == '/') {
          ksnprintf(buffer, sizeof(buffer), "%s\n", proc_name);
        } else {
          ksnprintf(buffer, sizeof(buffer), "/bin/%s\n", proc_name);
        }
        return make_proc_file(buffer);
      }

      if (strcmp(p, "/root") == 0 || strcmp(p, "root") == 0) {
        ksnprintf(buffer, sizeof(buffer), "/\n");
        return make_proc_file(buffer);
      }

      if (strcmp(p, "/maps") == 0 || strcmp(p, "maps") == 0) {
        if (t->process && t->process->mmap_next > 0) {
          ksnprintf(buffer, sizeof(buffer),
                    "00400000-00408000 r-xp 00000000 00:00 0                          [text]\n"
                    "50000000-%08lx rwxp 00000000 00:00 0                          [heap/mmap]\n"
                    "7fffff7fe000-7ffffffff000 rwxp 00000000 00:00 0                  [stack]\n",
                    t->process->mmap_next);
        } else {
          ksnprintf(buffer, sizeof(buffer),
                    "00400000-00408000 r-xp 00000000 00:00 0                          [text]\n"
                    "7fffff7fe000-7ffffffff000 rwxp 00000000 00:00 0                  [stack]\n");
        }
        return make_proc_file(buffer);
      }

      if (strcmp(p, "/fd") == 0 || strcmp(p, "fd") == 0 || strcmp(p, "/fd/") == 0 ||
          strcmp(p, "fd/") == 0) {
        size_t offset = 0;
        buffer[0] = 0;
        if (t->process) {
          for (int i = 0; i < 32; i++) {
            if (t->process->fds[i].type != sched::FD_NONE) {
              offset += ksnprintf(buffer + offset, sizeof(buffer) - offset, "%d\n", i);
            }
          }
        }
        return make_proc_file(buffer);
      }

      if (strncmp(p, "/fd/", 4) == 0 || strncmp(p, "fd/", 3) == 0) {
        const char *fd_str = (p[0] == '/') ? p + 4 : p + 3;
        int fd_num = 0;
        while (*fd_str >= '0' && *fd_str <= '9') {
          fd_num = fd_num * 10 + (*fd_str - '0');
          fd_str++;
        }

        const char *target = "unknown";
        if (t->process && fd_num >= 0 && fd_num < 32) {
          if (t->process->fds[fd_num].type == sched::FD_FILE && t->process->fds[fd_num].node) {
            target = t->process->fds[fd_num].node->name;
          } else if (t->process->fds[fd_num].type == sched::FD_PIPE) {
            target = "pipe";
          }
        }

        ksnprintf(buffer, sizeof(buffer), "%s\n", target);
        return make_proc_file(buffer);
      }

      if (strcmp(p, "/fdinfo") == 0 || strcmp(p, "fdinfo") == 0 || strcmp(p, "/fdinfo/") == 0 ||
          strcmp(p, "fdinfo/") == 0) {
        size_t offset = 0;
        buffer[0] = 0;
        if (t->process) {
          for (int i = 0; i < 32; i++) {
            if (t->process->fds[i].type != sched::FD_NONE) {
              offset += ksnprintf(buffer + offset, sizeof(buffer) - offset, "%d\n", i);
            }
          }
        }
        return make_proc_file(buffer);
      }

      if (strncmp(p, "/fdinfo/", 8) == 0 || strncmp(p, "fdinfo/", 7) == 0) {
        const char *fd_str = (p[0] == '/') ? p + 8 : p + 7;
        int fd_num = 0;
        while (*fd_str >= '0' && *fd_str <= '9') {
          fd_num = fd_num * 10 + (*fd_str - '0');
          fd_str++;
        }

        size_t pos = 0;
        if (t->process && fd_num >= 0 && fd_num < 32) {
          pos = t->process->fds[fd_num].offset;
        }

        ksnprintf(buffer, sizeof(buffer), "pos:\t%lu\nflags:\t02\nmnt_id:\t9\n", pos);
        return make_proc_file(buffer);
      }

      if (strcmp(p, "/statm") == 0 || strcmp(p, "statm") == 0) {
        uint64_t pages = count_user_pages(t->process ? t->process->space : nullptr);
        ksnprintf(buffer, sizeof(buffer), "%lu %lu %lu 1 0 %lu 0\n", pages, pages, pages,
                  pages > 0 ? pages - 1 : 0);
        return make_proc_file(buffer);
      }

      if (strcmp(p, "/environ") == 0 || strcmp(p, "environ") == 0) {
        ksnprintf(buffer, sizeof(buffer), "PATH=/bin:/usr/bin\n");
        return make_proc_file(buffer);
      }

      if (strcmp(p, "/wchan") == 0 || strcmp(p, "wchan") == 0) {
        const char *wchan_str = (t->state == sched::State::BLOCKED) ? "nanosleep" : "0";
        ksnprintf(buffer, sizeof(buffer), "%s\n", wchan_str);
        return make_proc_file(buffer);
      }

      if (strcmp(p, "/io") == 0 || strcmp(p, "io") == 0) {
        ksnprintf(buffer, sizeof(buffer),
                  "rchar: 0\n"
                  "wchar: 0\n"
                  "syscr: 0\n"
                  "syscw: 0\n"
                  "read_bytes: 0\n"
                  "write_bytes: 0\n"
                  "cancelled_write_bytes: 0\n");
        return make_proc_file(buffer);
      }

      if (strcmp(p, "/cgroup") == 0 || strcmp(p, "cgroup") == 0) {
        ksnprintf(buffer, sizeof(buffer), "0::/\n");
        return make_proc_file(buffer);
      }

      if (strcmp(p, "/task") == 0 || strcmp(p, "task") == 0 || strcmp(p, "/task/") == 0 ||
          strcmp(p, "task/") == 0) {
        ksnprintf(buffer, sizeof(buffer), "%lu\n", pid);
        return make_proc_file(buffer);
      }

      if (strcmp(p, "/mounts") == 0 || strcmp(p, "mounts") == 0) {
        return open(nullptr, "mounts");
      }

      if (strcmp(p, "/mountinfo") == 0 || strcmp(p, "mountinfo") == 0) {
        size_t offset = 0;
        buffer[0] = 0;
        int id = 1;
        for (fs::Mount *m = fs::get_mounts(); m; m = m->next) {
          const char *fs_name = (m->fs && m->fs->name) ? m->fs->name : "unknown";
          offset +=
              ksnprintf(buffer + offset, sizeof(buffer) - offset,
                        "%d 1 0:0 / %s rw,relatime - %s %s rw\n", id++, m->path, fs_name, fs_name);
        }
        return make_proc_file(buffer);
      }

      if (strcmp(p, "/auxv") == 0 || strcmp(p, "auxv") == 0) {
        return make_proc_file("");
      }
    }
  }

  return nullptr;
}

static int read(void *ptr, char *buf, size_t size) {
  ProcFile *file = (ProcFile *)ptr;

  if (!file || !buf)
    return -1;

  if (file->offset >= file->size)
    return 0;

  size_t remaining = file->size - file->offset;
  size_t n = size < remaining ? size : remaining;

  memcpy(buf, file->data + file->offset, n);
  file->offset += n;
  return n;
}

static void close(void *ptr) {
  ProcFile *file = (ProcFile *)ptr;

  if (!file)
    return;

  heap::kfree(file->data);
  heap::kfree(file);
}

static void list(void *) {
  for (int i = 0; entries[i].name; i++)
    kprintf("%s\n", entries[i].name);
  kprintf("self\n");
  for (sched::Task *t = sched::head; t; t = t->next) {
    kprintf("%lu\n", t->pid);
  }
}

static bool init(fs::FileSystem *fs) {
  kprintf("procfs init\n");

  vfs::Node *proc_dir = vfs::find("/proc");

  if (proc_dir) {
    for (int i = 0; entries[i].name; i++)
      vfs::create_node(entries[i].name, false, proc_dir);
    vfs::create_node("self", true, proc_dir);
  }

  return true;
}

FileSystem filesystem = {.name = "procfs",
                         .init = init,
                         .open = open,
                         .read = read,
                         .write = nullptr,
                         .close = close,
                         .list = list};

} // namespace fs::procfs
