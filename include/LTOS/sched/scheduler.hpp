#pragma once

#include "LTOS/sched/task.hpp"

namespace sched {

void init();

void create(uint64_t entry);

Registers *schedule(Registers *current);

Task *get_current();

void destroy_task(Task *task);

void exec(const char *path);
void yield();
void idle();
void exit();

Task *find(uint64_t pid);

Process *clone(Process *parent);

void add(Task *task);

} // namespace sched
