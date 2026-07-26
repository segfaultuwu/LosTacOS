#include <sched.h>
#include <sys/syscall.h>

int sched_yield(void) {
  return (int)syscall(SYS_YIELD, 0, 0, 0);
}

int sched_getscheduler(pid_t pid) {
  (void)pid;
  return SCHED_OTHER;
}

int sched_setscheduler(pid_t pid, int policy, void *param) {
  (void)pid;
  (void)policy;
  (void)param;

  return 0;
}
