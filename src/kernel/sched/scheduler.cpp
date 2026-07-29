#include "LTOS/sched/scheduler.hpp"
#include "LTOS/arch/x86_64/paging.hpp"
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

static uint64_t setup_user_stack(uint64_t stack_top, char **argv, char **envp);

static void task_wrapper(void (*entry)()) {

  entry();

  sched::exit();

  while (true)
    asm volatile("hlt");
}

uint64_t create_user_stack(mm::AddressSpace *space) {

  uint64_t stack_top = 0x7fffff7000;
  uint64_t stack_size = 0x10000; // 64KB

  void *top_page = nullptr;

  for (uint64_t addr = stack_top - stack_size; addr < stack_top; addr += 0x1000) {

    void *page = paging::alloc_page();

    paging::map_page(space->table->pml4, addr, (uint64_t)page,
                     PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);

    if (addr + 0x1000 == stack_top)
      top_page = page;
  }

  uint8_t *top = (uint8_t *)top_page + 0x1000;

  *(uint64_t *)(top - 8) = 0;  // envp
  *(uint64_t *)(top - 16) = 0; // argv
  *(uint64_t *)(top - 24) = 0; // argc

  return stack_top - 24;
}

static Task *create_task(Process *proc, uint64_t entry) {

  Task *task = (Task *)heap::kmalloc(sizeof(Task));

  if (!task)
    return nullptr;

  memset(task, 0, sizeof(Task));

  task->stack = (uint8_t *)heap::kmalloc(8192);

  if (!task->stack) {
    heap::kfree(task);
    return nullptr;
  }

  uint64_t user_stack = create_user_stack(proc->space);

  if (!user_stack) {
    heap::kfree(task->stack);
    heap::kfree(task);
    return nullptr;
  }

  user_stack &= ~0xFULL;

  // crt0's _start reads argc straight from [rsp] with no push first, so
  // rsp must already point at a valid argc/argv/envp frame inside mapped
  // memory -- not at the raw (unmapped) top-of-stack address.
  //
  // setup_user_stack() writes through that raw user-space VA directly, and
  // that mapping only exists in proc->space's own page tables -- it isn't
  // reachable through whatever table is currently active (kernel_task's,
  // or another process's). Switch to it for the duration of the write,
  // then switch back, same as exec_current() does.
  mm::AddressSpace *caller_space = current_task ? current_task->process->space : nullptr;

  proc->space->activate();

  user_stack = setup_user_stack(user_stack, nullptr, nullptr);

  if (caller_space)
    caller_space->activate();

  Registers *r = (Registers *)(task->stack + 8192 - sizeof(Registers));

  memset(r, 0, sizeof(Registers));

  r->rip = entry;
  r->rsp = user_stack;

  // user mode selectors
  r->cs = 0x1B;
  r->ss = 0x23;

  r->rflags = 0x202;

  r->vector = 0;
  r->error = 0;

  task->pid = pid_counter++;

  task->regs = r;

  task->state = State::READY;

  task->process = proc;

  task->next = nullptr;

  add(task);

  return task;
}

Process *create_process(uint64_t entry) {
  Process *proc = (Process *)heap::kmalloc(sizeof(Process));
  if (!proc)
    return nullptr;

  memset(proc, 0, sizeof(Process));

  proc->pid = pid_counter++;

  proc->space = mm::AddressSpace::create(); // albo clone kernel space

  Task *task = create_task(proc, entry);

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

void spawn(const char *path) {
  Process *proc = (Process *)heap::kmalloc(sizeof(Process));

  if (!proc)
    return;

  memset(proc, 0, sizeof(Process));

  proc->pid = pid_counter++;

  proc->space = mm::AddressSpace::create();

  if (!proc->space) {
    heap::kfree(proc);
    return;
  }

  uint64_t entry = elf::load(path, proc->space);

  // Note: the user stack is set up inside create_task() -> create_user_stack()
  // below; it used to also be mapped here, but that just leaked the physical
  // pages allocated for it since create_task() maps the same virtual range
  // again right after.

  if (!entry) {

    proc->space->destroy();

    heap::kfree(proc);

    logger::error("spawn failed %s", path);

    return;
  }

  Task *task = create_task(proc, entry);

  if (!task) {

    proc->space->destroy();

    heap::kfree(proc);

    return;
  }

  proc->main_thread = task;
}

static uint64_t setup_user_stack(uint64_t stack_top, char **argv, char **envp) {
  uint64_t sp = stack_top;

  char *argv_ptrs[64];
  char *env_ptrs[64];

  int argc = 0;
  int envc = 0;

  // copy string envp
  if (envp) {
    while (envp[envc] && envc < 63) {

      size_t len = strlen(envp[envc]) + 1;

      sp -= len;

      memcpy((void *)sp, envp[envc], len);

      env_ptrs[envc] = (char *)sp;

      envc++;
    }
  }

  // copy string argv
  if (argv) {
    while (argv[argc] && argc < 63) {

      size_t len = strlen(argv[argc]) + 1;

      sp -= len;

      memcpy((void *)sp, argv[argc], len);

      argv_ptrs[argc] = (char *)sp;

      argc++;
    }
  }

  /*
      SysV ABI:

      argc
      argv[]
      NULL
      envp[]
      NULL

  */

  sp &= ~0xFULL;

  // From here we push a fixed number of 8-byte slots: envp NULL, envc
  // envp pointers, argv NULL, argc argv pointers, and finally argc itself
  // -- that's 8 * (3 + envc + argc) bytes. For the final rsp (where argc
  // lands) to satisfy the ABI's rsp % 16 == 8 at entry, that total must be
  // == 8 (mod 16), i.e. (envc + argc) must be even. Pad here if it isn't --
  // NOT next to argc itself, since crt0 expects argv at exactly [rsp+8]
  // with no gap.
  if ((envc + argc) % 2 != 0) {
    sp -= sizeof(uint64_t);
  }

  // envp NULL
  sp -= sizeof(uint64_t);
  *(uint64_t *)sp = 0;

  // envp[]
  for (int i = envc - 1; i >= 0; i--) {

    sp -= sizeof(uint64_t);

    *(uint64_t *)sp = (uint64_t)env_ptrs[i];
  }

  uint64_t envp_addr = sp;

  // argv NULL
  sp -= sizeof(uint64_t);
  *(uint64_t *)sp = 0;

  // argv[]
  for (int i = argc - 1; i >= 0; i--) {

    sp -= sizeof(uint64_t);

    *(uint64_t *)sp = (uint64_t)argv_ptrs[i];
  }

  uint64_t argv_addr = sp;

  // argc -- lands at exactly the rsp we return, immediately below argv[0],
  // and (per the padding above) satisfies the ABI's rsp % 16 == 8 at entry.
  sp -= sizeof(uint64_t);

  *(uint64_t *)sp = argc;

  return sp;
}

extern "C" void exec_enter(Registers *r);

bool exec_current(const char *path, char **argv, char **envp) {

  Task *task = current_task;

  if (!task || !task->process)
    return false;

  Process *proc = task->process;

  mm::AddressSpace *new_space = mm::AddressSpace::create();

  if (!new_space)
    return false;

  uint64_t entry = elf::load(path, new_space);

  if (!entry) {

    new_space->destroy();

    return false;
  }

  mm::AddressSpace *old_space = proc->space;

  proc->space = new_space;

  new_space->activate();

  if (old_space)
    old_space->destroy();

  uint64_t stack_top = create_user_stack(proc->space);

  uint64_t stack = create_user_stack(proc->space);
  stack = setup_user_stack(stack, nullptr, nullptr);
  stack_top &= ~0xFULL;

  uint64_t user_stack = setup_user_stack(stack_top, argv, envp);

  Registers *r = (Registers *)(task->stack + 8192 - sizeof(Registers));

  memset(r, 0, sizeof(Registers));

  r->rip = entry;

  r->rsp = stack;

  r->cs = 0x1B;
  r->ss = 0x23;
  r->rflags = 0x202;

  r->vector = 0;
  r->error = 0;

  task->regs = r;

  exec_enter(r);

  __builtin_unreachable();
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

  paging::clone_user_pages(child->space->table->pml4, parent->space->table->pml4);

  Task *task = create_task(child, (uint64_t)task_wrapper);

  if (!task)
    return nullptr;

  memcpy(task->regs, parent->main_thread->regs, sizeof(Registers));

  // The Registers we just copied still hold the PARENT's rsp/rbp, i.e.
  // addresses inside the parent's own 8KB stack -- not the fresh one
  // create_task() just gave this child. Copy the live portion of the
  // parent's stack (from its current rsp up to the top) into the same
  // relative position in the child's own stack, and shift rsp/rbp by
  // the offset between the two stacks' top addresses, so the child's
  // saved registers point into its own copy instead of aliasing the
  // parent's live stack.
  uint64_t parent_top = ((uint64_t)parent->main_thread->stack + 8192) & ~0xFULL;
  uint64_t child_top = ((uint64_t)task->stack + 8192) & ~0xFULL;

  uint64_t parent_rsp = task->regs->rsp; // just copied from the parent

  uint64_t used = parent_top - parent_rsp;

  if (used > 8192)
    used = 8192; // defensive clamp -- shouldn't happen

  int64_t delta = (int64_t)child_top - (int64_t)parent_top;

  uint64_t child_rsp = (uint64_t)((int64_t)parent_rsp + delta);

  memcpy((void *)child_rsp, (void *)parent_rsp, used);

  task->regs->rsp = child_rsp;
  task->regs->rbp = (uint64_t)((int64_t)task->regs->rbp + delta);

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
