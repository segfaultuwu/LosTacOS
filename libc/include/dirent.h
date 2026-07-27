#pragma once

#include <stddef.h>
#include <stdint.h>

#define NAME_MAX 255

struct dirent {
  uint64_t d_ino;
  uint64_t d_off;

  uint16_t d_reclen;
  uint8_t d_type;

  char d_name[256];
};

typedef struct {
  int fd;
  struct dirent entry;
} DIR;

#define DT_UNKNOWN 0
#define DT_FIFO 1
#define DT_CHR 2
#define DT_DIR 4
#define DT_BLK 6
#define DT_REG 8
#define DT_LNK 10
#define DT_SOCK 12

DIR *opendir(const char *path);

struct dirent *readdir(DIR *dirp);

int closedir(DIR *dirp);
