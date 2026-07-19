#include "sys/syscall.h"

long syscall(long num, long a, long b, long c) {
  long ret;

  asm volatile("int $0x80" : "=a"(ret) : "a"(num), "D"(a), "S"(b), "d"(c));

  return ret;
}
