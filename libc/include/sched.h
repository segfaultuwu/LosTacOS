#pragma once

#include <sys/types.h>

int sched_yield(void);

int sched_getscheduler(pid_t pid);
int sched_setscheduler(pid_t pid, int policy, void *param);

#define SCHED_OTHER 0
#define SCHED_FIFO 1
#define SCHED_RR 2
