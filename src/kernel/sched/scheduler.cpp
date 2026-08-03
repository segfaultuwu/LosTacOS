#include "LTOS/sched/scheduler.hpp"
#include "LTOS/arch/x86_64/gdt.hpp"
#include "LTOS/exec/elf.hpp"
#include "LTOS/fs/vfs.hpp"
#include "LTOS/logger.hpp"
#include "LTOS/mm/address_space.hpp"
#include "LTOS/mm/heap.hpp"
#include "LTOS/mm/paging.hpp"
#include "LTOS/sched/process.hpp"
#include "LTOS/sched/task.hpp"

#include <stdint.h>
#include <string.h>

namespace sched {

Task *head = nullptr;
static Task *tail = nullptr;
static Task *current_task = nullptr;

static uint64_t pid_counter = 1;

void reap() {
  Task *t = head;

  while (t) {
    Task *next = t->next;

    if (t->state == State::DEAD && t != current_task) {
      remove_and_destroy(t);
    }

    t = next;
  }
}

static void kernel_idle() {
  while (true) {
    asm volatile("hlt");
  }
}

static uint64_t setup_user_stack(uint64_t stack_top, char **argv, char **envp);

static void task_wrapper(void (*entry)()) {

  entry();

  sched::exit(1);

  while (true)
    asm volatile("hlt");
}

uint64_t create_user_stack(mm::AddressSpace *space) {

  uint64_t stack_top = 0x8000000000;
  uint64_t stack_size = 0x40000; // 256KB

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

  task->pid = proc ? proc->pid : pid_counter++;

  task->regs = r;

  task->state = State::READY;

  task->process = proc;
  if (!proc->main_thread)
    proc->main_thread = task;

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

  proc->cwd = fs::vfs::find("/");

  proc->space = mm::AddressSpace::create();

  if (!proc->space) {
    heap::kfree(proc);
    return nullptr;
  }

  Task *task = create_task(proc, entry);

  if (!task) {
    proc->space->destroy();
    heap::kfree(proc);
    return nullptr;
  }

  proc->main_thread = task;

  return proc;
}

static Task kernel_task;

static Process kernel_process;

void init() {
  head = nullptr;

  kernel_process.pid = 0;
  kernel_process.space = mm::AddressSpace::kernel();
  kernel_process.cwd = fs::vfs::find("/");

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

static uint64_t setup_user_stack(uint64_t stack_top, char **argv, char **envp) {
  uint64_t sp = stack_top;

  char *argv_ptrs[64] = {};
  char *env_ptrs[64] = {};

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

  // Align stack pointer to 16 bytes for SysV ABI
  sp &= ~0xFULL;

  // Ensure total number of qwords pushed below leaves rsp % 16 == 0 (rsp % 16 == 8 before call
  // main) Total qwords = 1 (argc) + argc (argv) + 1 (NULL) + envc (envp) + 1 (NULL) = argc + envc +
  // 3
  if ((argc + envc + 3) % 2 != 0) {
    sp -= sizeof(uint64_t);
    *(uint64_t *)sp = 0;
  }

  // envp NULL
  sp -= sizeof(uint64_t);
  *(uint64_t *)sp = 0;

  // envp[]
  for (int i = envc - 1; i >= 0; i--) {
    sp -= sizeof(uint64_t);
    *(uint64_t *)sp = (uint64_t)env_ptrs[i];
  }

  // argv NULL
  sp -= sizeof(uint64_t);
  *(uint64_t *)sp = 0;

  // argv[]
  for (int i = argc - 1; i >= 0; i--) {
    sp -= sizeof(uint64_t);
    *(uint64_t *)sp = (uint64_t)argv_ptrs[i];
  }

  // argc
  sp -= sizeof(uint64_t);
  *(uint64_t *)sp = argc;

  return sp;
}

extern "C" void exec_enter(Registers *r);

static char *kstrdup(const char *s) {
  size_t len = strlen(s) + 1;

  char *out = (char *)heap::kmalloc(len);

  if (out)
    memcpy(out, s, len);

  return out;
}

bool exec_current(const char *path, char **argv, char **envp) {

  Task *task = current_task;

  if (!task || !task->process)
    return false;

  Process *proc = task->process;

  // argv/envp are user-space pointers that only make sense in the
  // CALLING process's address space, which we're about to tear down and
  // replace below. Copy everything into kernel memory now, while those
  // mappings are still active -- reading them after the switch would
  // dereference stale pointers into an address space that no longer
  // backs them (silently, since the old and new stacks happen to sit at
  // the same fixed VA, so this used to just read back garbage/zeroes
  // instead of faulting).
  char *argv_copy[64] = {};
  char *envp_copy[64] = {};

  if (argv) {
    for (int i = 0; argv[i] && i < 63; i++)
      argv_copy[i] = kstrdup(argv[i]);
  }

  if (envp) {
    for (int i = 0; envp[i] && i < 63; i++)
      envp_copy[i] = kstrdup(envp[i]);
  }

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
  proc->mmap_next = 0;
  strncpy(proc->name, path, sizeof(proc->name) - 1);
  proc->name[sizeof(proc->name) - 1] = '\0';

  new_space->activate();

  if (old_space)
    old_space->destroy();

  uint64_t stack_top = create_user_stack(proc->space);
  stack_top &= ~0xFULL;

  uint64_t user_stack = setup_user_stack(stack_top, argv_copy, envp_copy);

  for (int i = 0; argv_copy[i]; i++)
    heap::kfree(argv_copy[i]);

  for (int i = 0; envp_copy[i]; i++)
    heap::kfree(envp_copy[i]);

  Registers *r = (Registers *)(task->stack + 8192 - sizeof(Registers));

  memset(r, 0, sizeof(Registers));

  r->rip = entry;

  r->rsp = user_stack;

  r->cs = 0x1B;
  r->ss = 0x23;
  r->rflags = 0x202;

  r->vector = 0;
  r->error = 0;

  task->regs = r;

  if (task->stack) {
    gdt::set_kernel_stack((uint64_t)task->stack + 8192);
  }

  exec_enter(r);

  __builtin_unreachable();
}

Registers *schedule(Registers *old) {
  reap();
  if (current_task) {
    current_task->regs = old;
    if (current_task->state == State::RUNNING) {
      current_task->state = State::READY;
    }
  }

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
    if (next->state == State::READY)
      break;

    next = next->next;
    if (!next)
      next = head;

  } while (next != start);

  if (next->state != State::READY) {
    Task *t = head;

    do {
      if (t->state != State::DEAD) {
        current_task = t;
        t->state = State::RUNNING;
        return t->regs;
      }

      t = t->next ? t->next : head;

    } while (t != head);

    return old;
  }

  current_task = next;

  current_task->state = State::RUNNING;

  if (current_task->stack) {
    gdt::set_kernel_stack((uint64_t)current_task->stack + 8192);
  }

  if (current_task->process && current_task->process->space) {
    current_task->process->space->activate();
  }

  return current_task->regs;
}

Task *get_current() {
  return current_task;
}

extern "C" void set_current_regs(Registers *r) {
  if (current_task)
    current_task->regs = r;
}

Task *find(uint64_t pid) {
  for (Task *t = head; t; t = t->next) {
    if (t->pid == pid)
      return t;
  }

  return nullptr;
}

Task *spawn(const char *path, char **argv) {
  Process *proc = (Process *)heap::kmalloc(sizeof(Process));
  if (!proc)
    return nullptr;

  memset(proc, 0, sizeof(Process));

  proc->pid = pid_counter++;
  proc->cwd = fs::vfs::find("/");
  strncpy(proc->name, path, sizeof(proc->name) - 1);
  proc->name[sizeof(proc->name) - 1] = '\0';

  proc->space = mm::AddressSpace::create();
  if (!proc->space)
    return nullptr;

  uint64_t entry = elf::load(path, proc->space);
  if (!entry)
    return nullptr;

  Task *task = create_task(proc, entry);
  if (!task)
    return nullptr;

  task->state = State::READY;
  task->parent = current_task;
  task->exit_code = 0;

  return task;
}

void yield() {
  asm volatile("int $32");
}

void exit(int code) {
  if (!current_task)
    return;

  current_task->exit_code = code;
  current_task->state = State::DEAD;

  if (current_task->parent && current_task->parent->state == State::BLOCKED) {
    current_task->parent->state = State::READY;
  }

  Registers *next_regs = schedule(current_task->regs);

  exec_enter(next_regs);

  while (true) {
    asm volatile("sti; hlt");
  }
}

Process *clone(Process *parent) {
  if (!parent || !parent->main_thread)
    return nullptr;

  Process *child = (Process *)heap::kmalloc(sizeof(Process));
  if (!child)
    return nullptr;

  memset(child, 0, sizeof(Process));

  child->pid = pid_counter++;
  child->space = mm::AddressSpace::create();

  if (!child->space) {
    heap::kfree(child);
    return nullptr;
  }

  strncpy(child->name, parent->name, sizeof(child->name) - 1);
  child->mmap_next = parent->mmap_next;
  child->cwd = parent->cwd;
  child->parent = parent;

  memcpy(child->fds, parent->fds, sizeof(child->fds));
  for (int i = 0; i < 32; i++) {
    if (child->fds[i].type == FD_PIPE && child->fds[i].pipe) {
      if (child->fds[i].readable)
        child->fds[i].pipe->readers++;
      if (child->fds[i].writable)
        child->fds[i].pipe->writers++;
    }
  }

  if (!paging::clone_user_pages(child->space->table->pml4, parent->space->table->pml4)) {
    child->space->destroy();
    heap::kfree(child);
    return nullptr;
  }

  Task *task = (Task *)heap::kmalloc(sizeof(Task));
  if (!task) {
    child->space->destroy();
    heap::kfree(child);
    return nullptr;
  }

  memset(task, 0, sizeof(Task));

  task->stack = (uint8_t *)heap::kmalloc(8192);
  if (!task->stack) {
    child->space->destroy();
    heap::kfree(task);
    heap::kfree(child);
    return nullptr;
  }

  task->pid = child->pid;
  task->process = child;
  task->parent = parent->main_thread;
  task->state = State::READY;

  Registers *r = (Registers *)(task->stack + 8192 - sizeof(Registers));
  memcpy(r, parent->main_thread->regs, sizeof(Registers));

  // fork() returns 0 in child
  r->rax = 0;

  task->regs = r;
  child->main_thread = task;

  add(task);

  return child;
}

void add(Task *task) {
  if (!task)
    return;

  task->next = nullptr;

  if (!head) {
    head = task;
    tail = task;
    return;
  }

  tail->next = task;
  tail = task;
}

void destroy_task(Task *task) {

  if (!task)
    return;

  if (task->stack)
    heap::kfree(task->stack);

  heap::kfree(task);
}

void remove_and_destroy(Task *task) {
  if (!task || task == current_task)
    return;

  if (head == task) {
    head = task->next;
    if (tail == task)
      tail = head;
  } else {
    Task *t = head;
    while (t && t->next != task)
      t = t->next;
    if (t) {
      t->next = task->next;
      if (tail == task)
        tail = t;
    }
  }

  if (task->process) {
    if (task->process->space) {
      task->process->space->destroy();
    }
    heap::kfree(task->process);
  }

  destroy_task(task);
}

} // namespace sched
