#include <dirent.h>
#include <stdlib.h>
#include <unistd.h>

DIR *opendir(const char *path) {
  int fd = open(path);

  if (fd < 0)
    return 0;

  DIR *dir = malloc(sizeof(DIR));

  if (!dir) {
    close(fd);
    return 0;
  }

  dir->fd = fd;

  return dir;
}

struct dirent *readdir(DIR *dirp) {
  if (!dirp)
    return 0;

  if (sys_readdir(dirp->fd, &dirp->entry))
    return &dirp->entry;

  return 0;
}

int closedir(DIR *dirp) {
  if (!dirp)
    return -1;

  close(dirp->fd);

  free(dirp);

  return 0;
}
