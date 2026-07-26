#include <sys/mman.h>
#include <sys/syscall.h>

void *mmap(void *addr, size_t length, int prot, int flags, int fd, long offset) {
  long ret = syscall(SYS_MMAP, (long)addr, length, ((long)prot << 32) | flags);

  if (ret < 0)
    return MAP_FAILED;

  return (void *)ret;
}

int munmap(void *addr, size_t length) {
  return (int)syscall(SYS_MUNMAP, (long)addr, length, 0);
}

int mprotect(void *addr, size_t len, int prot) {
  (void)addr;
  (void)len;
  (void)prot;

  return -1;
}
