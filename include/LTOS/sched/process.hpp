#pragma once

#include <stdint.h>

#include "LTOS/arch/x86_64/paging.hpp"
#include "LTOS/fs/vfs.hpp"
#include "LTOS/sched/state.hpp"

namespace sched {

struct Task;

struct Process {

  uint64_t pid;

  char name[32];

  Task *main_thread;

  paging::PageTable *space;

  fs::vfs::Node *cwd;

  int exit_code;

  State state;

  Process *parent;
};

} // namespace sched
