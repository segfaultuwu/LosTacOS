#include <stddef.h>
#include <sys/syscall.h>

void *malloc(size_t size) {
  if (size == 0)
    return NULL;

  return (void *)syscall(SYS_MMAP, 0, size, 0);
}

void free(void *ptr) {
  if (!ptr)
    return;

  syscall(SYS_MUNMAP, (long)ptr, 0, 0);
}
