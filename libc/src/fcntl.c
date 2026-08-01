#include <fcntl.h>
#include <stdarg.h>
#include <sys/syscall.h>

int fcntl(int fd, int cmd, ...) {
  va_list args;
  va_start(args, cmd);
  long arg = va_arg(args, long);
  va_end(args);

  return (int)syscall(SYS_FCNTL, fd, cmd, arg);
}
