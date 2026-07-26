#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>

#define SYS_OPENDIR 18
#define SYS_READDIR 19
#define SYS_CLOSEDIR 20

DIR *opendir(const char *path) {
  int fd = (int)syscall(SYS_OPENDIR, (long)path, 0, 0);

  if (fd < 0)
    return NULL;

  DIR *dir = malloc(sizeof(DIR));

  if (!dir)
    return NULL;

  dir->fd = fd;

  memset(&dir->entry, 0, sizeof(struct dirent));

  return dir;
}

struct dirent *readdir(DIR *dirp) {
  if (!dirp)
    return NULL;

  long ret = syscall(SYS_READDIR, dirp->fd, (long)&dirp->entry, sizeof(struct dirent));

  if (ret <= 0)
    return NULL;

  return &dirp->entry;
}

int closedir(DIR *dirp) {
  if (!dirp)
    return -1;

  int ret = (int)syscall(SYS_CLOSEDIR, dirp->fd, 0, 0);

  free(dirp);

  return ret;
}
