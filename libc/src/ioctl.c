#include <sys/ioctl.h>
#include <sys/syscall.h>

int ioctl(int fd, unsigned long req, void *arg) {
  return syscall(SYS_IOCTL, fd, req, (long)arg);
}
