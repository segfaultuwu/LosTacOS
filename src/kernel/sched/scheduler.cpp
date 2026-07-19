#include "LTOS/sched/scheduler.hpp"
#include "LTOS/drivers/serial.hpp"
#include "LTOS/exec/elf.hpp"
#include "LTOS/logger.hpp"
#include "LTOS/mm/heap.hpp"
#include "LTOS/sched/task.hpp"
#include <cstdint>
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

void create(uint64_t entry) {

  task_count++;

  Task *task = (Task *)heap::kmalloc(sizeof(Task));

  if (!task) {
    logger::error("Scheduler: failed allocating task");
    return;
  }

  task->stack = (uint8_t *)heap::kmalloc(8192);

  if (!task->stack) {
    logger::error("Scheduler: failed allocating stack");
    heap::kfree(task);
    return;
  }

  uint64_t stack = (uint64_t)task->stack + 8192;

  stack -= sizeof(Registers);

  Registers *r = (Registers *)stack;

  memset(r, 0, sizeof(Registers));

  r->rip = entry;

  r->cs = 0x08;

  r->rflags = 0x202;

  r->rsp = stack;

  r->ss = 0x10;

  task->pid = pid++;

  task->regs = r;

  task->state = State::READY;

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

void exec(const char *path) {
  uint64_t entry = elf::load(path);

  if (!entry) {
    logger::error("exec: failed to load %s", path);
    return;
  }

  sched::create(entry);
}

Registers *schedule(Registers *old) {
  if (current_task)
    current_task->regs = old;

  Task *next = current_task;

  do {

    if (!next)
      next = head;
    else
      next = next->next;

    if (!next)
      next = head;
    destroy_task(current_task);

  } while (next->state == State::DEAD);

  current_task = next;

  current_task->state = State::RUNNING;

  return current_task->regs;
}

static void task_wrapper(void (*entry)()) {
  entry();

  while (true)
    asm volatile("hlt");
}

void destroy_task(Task *task) {
  heap::kfree(task->stack);
  heap::kfree(task);
}

Task *get_current() {
  return current_task;
}

void yield() {
  asm volatile("int $32");
}

void idle() {
  while (true)
    asm volatile("hlt");
}

void exit() {
  if (!current_task)
    return;

  current_task->state = State::DEAD;

  asm volatile("int $32");

  while (true)
    asm volatile("hlt");
}

} // namespace sched
