#pragma once

#include <stdint.h>

#include "LTOS/fs/vfs.hpp"
#include "LTOS/mm/address_space.hpp"
#include "LTOS/mm/paging.hpp"
#include "LTOS/sched/state.hpp"

namespace sched {

struct Task;

enum FDType { FD_NONE, FD_FILE, FD_PIPE, FD_TTY };

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
  bool nonblock = false;
  int flags = 0;
};

struct Process {

  uint64_t pid;

  char name[32];

  Task *main_thread;

  mm::AddressSpace *space;

  uint64_t mmap_next; // bump pointer for sys_mmap's per-process user-space
                      // allocations; 0 until first use, then lazily
                      // initialized to MMAP_BASE (see syscall.cpp)

  fs::vfs::Node *cwd;

  int exit_code;

  State state;

  Process *parent;

  FdEntry fds[32];
};

} // namespace sched
