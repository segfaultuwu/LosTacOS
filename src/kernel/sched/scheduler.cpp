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

static uint64_t pid_counter = 1;

void init() {
  head = nullptr;
  current_task = nullptr;

  logger::info("Scheduler initialized");
}

static void task_wrapper() {
  logger::info("task finished");

  sched::exit();

  while (true)
    asm volatile("hlt");
}

void create(uint64_t entry) {

  Task *task = (Task *)heap::kmalloc(sizeof(Task));

  if (!task)
    return;

  task->stack = (uint8_t *)heap::kmalloc(8192);

  if (!task->stack) {
    heap::kfree(task);
    return;
  }

  uint64_t stack_top = (uint64_t)task->stack + 8192;

  stack_top &= ~0xFULL;

  Registers *r = (Registers *)(stack_top - sizeof(Registers));

  memset(r, 0, sizeof(Registers));

  r->rip = entry;
  r->cs = 0x08;
  r->rflags = 0x202;
  r->rsp = stack_top;
  r->ss = 0x10;

  task->pid = pid_counter++;

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

  logger::info("created pid=%d rip=%lx rsp=%lx", task->pid, r->rip, r->rsp);
}

void exec(const char *path) {

  uint64_t entry = elf::load(path);

  if (!entry) {

    logger::error("exec: failed loading %s", path);

    return;
  }

  create(entry);
}

Registers *schedule(Registers *old) {

  if (current_task) {

    current_task->regs = old;
  }

  if (!head) {

    return old;
  }

  Task *next = nullptr;

  if (!current_task) {

    next = head;

  } else {

    next = current_task->next;

    if (!next)
      next = head;
  }

  /*
      skip dead tasks
  */

  Task *start = next;

  while (next->state == State::DEAD) {

    next = next->next;

    if (!next)
      next = head;

    if (next == start)
      return old;
  }

  current_task = next;

  current_task->state = State::RUNNING;

  // logger::info("switch pid=%d rip=%lx", current_task->pid, current_task->regs->rip);

  return current_task->regs;
}

void destroy_task(Task *task) {

  if (!task)
    return;

  if (task->stack)
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

  logger::info("task %d exited", current_task->pid);

  current_task->state = State::DEAD;

  asm volatile("int $32");

  while (true)
    asm volatile("hlt");
}

} // namespace sched
