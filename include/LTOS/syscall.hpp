#pragma once

#include <stdint.h>

enum Syscall {
  SYS_WRITE = 1,
  SYS_READ,
  SYS_EXIT,
  SYS_OPEN,
  SYS_CLOSE,
  SYS_FORK,
  SYS_EXEC,
  SYS_GETPID,
  SYS_YIELD,
  SYS_SLEEP,
  SYS_LSEEK,
  SYS_FSIZE,
  SYS_WAIT,
};

extern "C" uint64_t syscall_handler(uint64_t num, uint64_t a, uint64_t b, uint64_t c);
