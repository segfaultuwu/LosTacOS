#include <sys/syscall.h>
#include <unistd.h>

int open(const char *path) {
  return (int)syscall(SYS_OPEN, (long)path, 0, 0);
}

int close(int fd) {
  return (int)syscall(SYS_CLOSE, fd, 0, 0);
}

long read(int fd, void *buf, size_t len) {
  return syscall(SYS_READ, fd, (long)buf, (long)len);
}

long write(int fd, const void *buf, size_t len) {
  return syscall(SYS_WRITE, fd, (long)buf, (long)len);
}

int exec(const char *path) {
  return (int)syscall(SYS_EXEC, (long)path, 0, 0);
}

int getpid(void) {
  return (int)syscall(SYS_GETPID, 0, 0, 0);
}

int yield(void) {
  return (int)syscall(SYS_YIELD, 0, 0, 0);
}

int fork(void) {
  return syscall(SYS_FORK, 0, 0, 0);
}

void sleep_ms(unsigned long ms) {
  syscall(SYS_SLEEP, (long)ms, 0, 0);
}

long wait(int pid) {
  return syscall(SYS_WAIT, pid, 0, 0);
}

long lseek(int fd, long offset, int whence) {
  return syscall(SYS_LSEEK, fd, offset, whence);
}

long fsize(int fd) {
  return syscall(SYS_FSIZE, fd, 0, 0);
}
