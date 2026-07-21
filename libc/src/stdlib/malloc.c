#include <stddef.h>

#include <stddef.h>
#include <sys/syscall.h>

void *malloc(size_t size) {
  return (void *)syscall(SYS_MMAP, size, 0, 0);
}

void free(void *ptr) {
  if (!ptr)
    return;

  syscall(SYS_MUNMAP, (long)ptr, 0, 0);
}
