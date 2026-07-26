#include <sys/resource.h>

int getrlimit(int resource, struct rlimit *rlim) {
  if (!rlim)
    return -1;

  rlim->rlim_cur = RLIM_INFINITY;
  rlim->rlim_max = RLIM_INFINITY;

  return 0;
}

int setrlimit(int resource, const struct rlimit *rlim) {
  return 0;
}
