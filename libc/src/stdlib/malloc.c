#include <stddef.h>
#include <stdlib.h>
#include <string.h>
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

void *calloc(size_t count, size_t size) {
  size_t total = count * size;
  if (total == 0) return NULL;
  void *ptr = malloc(total);
  if (ptr) memset(ptr, 0, total);
  return ptr;
}

void *realloc(void *ptr, size_t size) {
  if (!ptr) return malloc(size);
  if (size == 0) {
    free(ptr);
    return NULL;
  }
  void *new_ptr = malloc(size);
  if (new_ptr && ptr) {
    memcpy(new_ptr, ptr, size);
    free(ptr);
  }
  return new_ptr;
}
