#include <sys/stat.h>

int stat(const char *path, struct stat *buf) {
  if (!buf)
    return -1;

  return -1;
}

int lstat(const char *path, struct stat *buf) {
  if (!buf)
    return -1;

  return -1;
}

int fstat(int fd, struct stat *buf) {
  if (!buf)
    return -1;

  return -1;
}

int chmod(const char *path, mode_t mode) {
  return -1;
}

int mkdir(const char *path, mode_t mode) {
  return -1;
}
