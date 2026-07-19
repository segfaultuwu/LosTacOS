#pragma once

#include "task.hpp"

namespace sched {

void init();

void create(void (*entry)());

Registers *schedule(Registers *current);

Task *get_current();

void destroy_task(Task *task);

void exec(const char *path);
void yield();
void idle();
void exit();

} // namespace sched
