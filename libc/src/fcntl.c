#include <fcntl.h>
#include <stdarg.h>
#include <sys/syscall.h>

int fcntl(int fd, int cmd, ...) {
  long arg = 0;
  if (cmd == F_SETFL || cmd == F_SETFD || cmd == F_DUPFD || cmd == F_DUPFD_CLOEXEC) {
    va_list args;
    va_start(args, cmd);
    arg = va_arg(args, long);
    va_end(args);
  }

  return (int)syscall(SYS_FCNTL, fd, cmd, arg);
}
