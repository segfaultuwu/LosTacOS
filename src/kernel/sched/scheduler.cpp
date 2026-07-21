#include "LTOS/sched/scheduler.hpp"
#include "LTOS/exec/elf.hpp"
#include "LTOS/logger.hpp"
#include "LTOS/mm/address_space.hpp"
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

static Task *create_task(Process *proc, void *entry, void *arg) {

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

  task->process = proc;

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

Process *create_process(uint64_t entry) {
  Process *proc = (Process *)heap::kmalloc(sizeof(Process));
  if (!proc)
    return nullptr;

  memset(proc, 0, sizeof(Process));

  proc->pid = pid_counter++;

  proc->space = mm::AddressSpace::create(); // albo clone kernel space

  Task *task = create_task(proc, (void *)task_wrapper, (void *)entry);

  proc->main_thread = task;

  return proc;
}

static Task kernel_task;

static Process kernel_process;

void init() {
  head = nullptr;

  kernel_process.pid = 0;
  kernel_process.space = mm::AddressSpace::kernel();

  kernel_task.pid = 0;
  kernel_task.process = &kernel_process;
  kernel_task.regs = nullptr;
  kernel_task.stack = nullptr;
  kernel_task.state = State::RUNNING;
  kernel_task.next = nullptr;

  current_task = &kernel_task;
}

void create(uint64_t entry) {
  create_process(entry);
}

void exec(const char *path) {
  Process *proc = (Process *)heap::kmalloc(sizeof(Process));
  if (!proc)
    return;

  memset(proc, 0, sizeof(Process));

  proc->pid = pid_counter++;

  proc->space = mm::AddressSpace::create();

  uint64_t entry = elf::load(path, proc->space);

  if (!entry) {
    logger::error("exec failed %s", path);
    return;
  }

  Task *task = create_task(proc, (void *)task_wrapper, (void *)entry);

  proc->main_thread = task;
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

Task *find(uint64_t pid) {
  for (Task *t = head; t; t = t->next) {
    if (t->pid == pid)
      return t;
  }

  return nullptr;
}

void yield() {
  asm volatile("int $32");
}

void exit() {
  if (!current_task || current_task->pid == 0)
    return;

  current_task->state = State::DEAD;

  if (current_task->process && current_task->process->space) {
    current_task->process->space->destroy();
  }

  asm volatile("int $32");

  while (1)
    asm volatile("hlt");
}

Process *clone(Process *parent) {
  if (!parent || !parent->main_thread)
    return nullptr;

  Process *child = (Process *)heap::kmalloc(sizeof(Process));

  if (!child)
    return nullptr;

  memcpy(child, parent, sizeof(Process));

  // memcpy above copies the parent's pid and space pointer verbatim.
  // Every process here calls exec() right after fork() (see bin/hello.c),
  // and exec() maps the new program straight into proc->space -- if that's
  // still literally the parent's AddressSpace, exec() ends up overwriting
  // the parent's own memory while the parent is still running. Give the
  // child its own address space (and its own pid, so it isn't mistaken
  // for the parent) instead of aliasing the parent's.
  child->pid = pid_counter++;
  child->space = mm::AddressSpace::create();

  if (!child->space) {
    heap::kfree(child);
    return nullptr;
  }

  Task *task = create_task(child, (void *)task_wrapper, nullptr);

  if (!task)
    return nullptr;

  memcpy(task->regs, parent->main_thread->regs, sizeof(Registers));

  // fork() == 0 in child
  task->regs->rax = 0;

  child->main_thread = task;

  return child;
}

void add(Task *task) {
  if (!task)
    return;

  task->next = nullptr;

  if (!head) {
    head = task;
    return;
  }

  Task *t = head;

  while (t->next)
    t = t->next;

  t->next = task;
}

void destroy_task(Task *task) {

  if (!task)
    return;

  if (task->stack)
    heap::kfree(task->stack);

  heap::kfree(task);
}

} // namespace sched
