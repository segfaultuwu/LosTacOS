#include <sys/mount.h>
#include <sys/syscall.h>

int mount(const char *source, const char *target, const char *filesystemtype, unsigned long flags,
          const void *data) {
  return (int)syscall(SYS_MOUNT, (long)source, (long)target, (long)filesystemtype, (long)flags,
                      (long)data);
}

int umount(const char *target) {
  return (int)syscall(SYS_UMOUNT2, (long)target, 0);
}

int umount2(const char *target, int flags) {
  return (int)syscall(SYS_UMOUNT2, (long)target, (long)flags);
}

