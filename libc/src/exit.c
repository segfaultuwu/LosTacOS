#include <stdlib.h>
#include <sys/syscall.h>

#define ATEXIT_MAX 32

typedef void (*atexit_func_t)(void);

static atexit_func_t atexit_funcs[ATEXIT_MAX];
static int atexit_count = 0;

int atexit(void (*func)(void)) {
  if (!func || atexit_count >= ATEXIT_MAX)
    return -1;
  atexit_funcs[atexit_count++] = func;
  return 0;
}

void exit(int code) {
  while (atexit_count > 0) {
    atexit_count--;
    if (atexit_funcs[atexit_count]) {
      atexit_funcs[atexit_count]();
    }
  }

  syscall(SYS_EXIT, code, 0, 0);

  while (1)
    ;
}
