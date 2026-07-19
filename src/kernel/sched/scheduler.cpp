#include "LTOS/drivers/serial.hpp"
#include "LTOS/logger.hpp"
#include "LTOS/mm/heap.hpp"
#include "LTOS/sched/task.hpp"
#include <string.h>

namespace sched {

static Task *head = nullptr;

static Task *current_task = nullptr;

static int task_count;

static uint64_t pid = 1;

void init() {
  current_task = nullptr;
  task_count = 0;
}

void create(void (*entry)()) {

  task_count++;

  Task *task = (Task *)heap::kmalloc(sizeof(Task));

  task->stack = (uint8_t *)heap::kmalloc(8192);

  uint64_t stack = (uint64_t)task->stack + 8192;

  stack -= sizeof(Registers);

  Registers *r = (Registers *)stack;

  memset(r, 0, sizeof(Registers));

  r->rip = (uint64_t)entry;

  r->cs = 0x08;

  r->rflags = 0x202;

  r->rsp = stack;

  r->ss = 0x10;

  task->pid = pid++;

  task->regs = r;

  task->state = READY;

  task->next = nullptr;

  if (!head)
    head = task;

  else {

    Task *t = head;

    while (t->next)
      t = t->next;

    t->next = task;
  }
}

Registers *schedule(Registers *old) {

  if (current_task)
    current_task->regs = old;

  if (!current_task)
    current_task = head;

  else
    current_task = current_task->next;

  if (!current_task)
    current_task = head;

  current_task->state = RUNNING;

  return current_task->regs;
}

Task *get_current() {
  return current_task;
}

} // namespace sched
