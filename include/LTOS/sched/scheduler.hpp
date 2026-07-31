#pragma once

#include "LTOS/sched/task.hpp"

namespace sched {

void init();

void create(uint64_t entry);

Registers *schedule(Registers *current);

Task *get_current();
extern Task *head;

void destroy_task(Task *task);
void remove_and_destroy(Task *task);

extern "C" void set_current_regs(Registers *r);

// Boot-time only: spawns a brand-new process running `path`. Used to launch
// the very first userspace process (init) when there is no current userspace
// process to replace.
Task *spawn(const char *path, char **argv);

bool exec_current(const char *path, char **argv, char **envp);
void yield();
void idle();
void exit(int code);

Task *find(uint64_t pid);

Process *clone(Process *parent);

void add(Task *task);

} // namespace sched
