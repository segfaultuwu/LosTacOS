#pragma once

#include <stdint.h>

#include "LTOS/arch/x86_64/paging.hpp"
#include "LTOS/fs/vfs.hpp"
#include "LTOS/mm/address_space.hpp"
#include "LTOS/sched/state.hpp"

namespace sched {

struct Task;

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
};

} // namespace sched
