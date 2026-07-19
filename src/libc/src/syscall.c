#include <stdint.h>

uint64_t syscall(uint64_t n, uint64_t a, uint64_t b, uint64_t c) {
  uint64_t ret;

  __asm__ volatile("int $0x80" : "=a"(ret) : "a"(n), "b"(a), "c"(b), "d"(c));

  return ret;
}
