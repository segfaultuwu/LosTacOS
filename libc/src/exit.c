#include "sys/syscall.h"

void exit(int code) {
  syscall(SYS_EXIT, code, 0, 0);

  while (1)
    ;
}
