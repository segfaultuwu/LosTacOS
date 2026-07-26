#pragma once

#include "LTOS/sched/task.hpp"

namespace sched {

void init();

void create(uint64_t entry);

Registers *schedule(Registers *current);

Task *get_current();

void destroy_task(Task *task);

// Boot-time only: spawns a brand-new process running `path`. Used to launch
// the very first userspace process (init) when there is no current userspace
// process to replace.
void spawn(const char *path);

// The exec() syscall: replaces the CALLING task's own program image in place
// (same pid, same Task/Process objects) instead of spawning an unrelated
// second process. Does not return on success -- control passes directly to
// the new program. Returns false if the load failed, in which case the
// caller's old image is left untouched.
bool exec_current(const char *path);
void yield();
void idle();
void exit();

Task *find(uint64_t pid);

Process *clone(Process *parent);

void add(Task *task);

} // namespace sched
