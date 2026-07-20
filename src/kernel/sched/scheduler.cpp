#include "LTOS/sched/scheduler.hpp"
#include "LTOS/exec/elf.hpp"
#include "LTOS/logger.hpp"
#include "LTOS/mm/heap.hpp"
#include "LTOS/sched/process.hpp"
#include "LTOS/sched/task.hpp"

#include <cstdint>
#include <string.h>

namespace sched {

static Task *head = nullptr;
static Task *current_task = nullptr;

static uint64_t pid_counter = 1;

static void kernel_idle() {
  while (true) {
    asm volatile("hlt");
  }
}

static void task_wrapper(void (*entry)()) {

  entry();

  logger::info("task returned");

  sched::exit();

  while (true)
    asm volatile("hlt");
}

static Task *create_task(void *entry, void *arg) {

  Task *task = (Task *)heap::kmalloc(sizeof(Task));

  if (!task)
    return nullptr;

  memset(task, 0, sizeof(Task));

  task->stack = (uint8_t *)heap::kmalloc(8192);

  if (!task->stack) {
    heap::kfree(task);
    return nullptr;
  }

  uint64_t stack_top = (uint64_t)task->stack + 8192;

  stack_top &= ~0xFULL;

  Registers *r = (Registers *)(stack_top - sizeof(Registers));

  memset(r, 0, sizeof(Registers));

  r->rip = (uint64_t)entry;

  r->rdi = (uint64_t)arg;

  r->rsp = stack_top;

  r->cs = 0x08;

  r->ss = 0x10;

  r->rflags = 0x202;

  task->pid = pid_counter++;

  task->regs = r;

  task->state = State::READY;

  task->next = nullptr;

  if (!head) {
    head = task;
  } else {

    Task *t = head;

    while (t->next)
      t = t->next;

    t->next = task;
  }

  return task;
}

static Task kernel_task;

void init() {
  head = nullptr;

  kernel_task.pid = 0;
  kernel_task.regs = nullptr;
  kernel_task.stack = nullptr;
  kernel_task.state = State::RUNNING;
  kernel_task.next = nullptr;

  current_task = &kernel_task;

  logger::info("Scheduler initialized");
}

void create(uint64_t entry) {

  Task *task = create_task((void *)task_wrapper, (void *)entry);

  if (!task)
    return;

  // logger::info("created pid=%d entry=%lx", task->pid, entry);
}

void exec(const char *path) {

  uint64_t entry = elf::load(path);

  if (!entry) {

    logger::error("exec failed %s", path);

    return;
  }

  create(entry);
}

Registers *schedule(Registers *old) {

  if (current_task)
    current_task->regs = old;

  if (!head)
    return old;

  Task *next;

  if (!current_task)
    next = head;

  else {

    next = current_task->next;

    if (!next)
      next = head;
  }

  Task *start = next;

  do {

    if (next->state != State::DEAD)
      break;

    next = next->next;

    if (!next)
      next = head;

  } while (next != start);

  current_task = next;

  current_task->state = State::RUNNING;

  if (current_task->process) {
    current_task->process->space->activate();
  }

  return current_task->regs;
}

Task *get_current() {
  return current_task;
}

void yield() {
  asm volatile("int $32");
}

void exit() {
  if (!current_task || current_task->pid == 0)
    return;

  current_task->state = State::DEAD;
  asm volatile("int $32");
  while (1)
    asm volatile("hlt");
}

void destroy_task(Task *task) {

  if (!task)
    return;

  if (task->stack)
    heap::kfree(task->stack);

  heap::kfree(task);
}

} // namespace sched
