#include <sys/statvfs.h>

int statvfs(const char *path, struct statvfs *buf) {
  if (!buf)
    return -1;

  buf->f_bsize = 4096;
  buf->f_frsize = 4096;
  buf->f_blocks = 0;
  buf->f_bfree = 0;
  buf->f_bavail = 0;

  return 0;
}

int fstatvfs(int fd, struct statvfs *buf) {
  return statvfs(0, buf);
}
