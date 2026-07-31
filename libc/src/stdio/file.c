#include <stdio.h>
#include <unistd.h>

FILE *fopen(const char *path, const char *mode) {
  int flags = 0;

  if (!mode)
    return NULL;

  if (mode[0] == 'r')
    flags = 0;
  else if (mode[0] == 'w')
    flags = 1;
  else if (mode[0] == 'a')
    flags = 2;
  else
    return NULL;

  int fd = open(path, flags);

  if (fd < 0)
    return NULL;

  FILE *file = malloc(sizeof(FILE));

  if (!file) {
    close(fd);
    return NULL;
  }

  file->fd = fd;
  file->eof = 0;
  file->error = 0;

  file->buffer = NULL;
  file->buffer_size = 0;
  file->buffer_pos = 0;

  return file;
}

int fclose(FILE *stream) {
  if (!stream)
    return -1;

  int ret = close(stream->fd);

  free(stream);

  return ret;
}

int fflush(FILE *stream) {
  /*
   * Nothing for now (TODO)
   */

  (void)stream;

  return 0;
}
