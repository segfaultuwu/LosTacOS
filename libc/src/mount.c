#include <sys/mount.h>

int mount(const char *source, const char *target, const char *filesystemtype, unsigned long flags,
          const void *data) {
  return -1;
}

int umount(const char *target) {
  return -1;
}
