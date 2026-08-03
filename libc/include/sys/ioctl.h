#pragma once

#include <stdint.h>

#define TIOCGWINSZ 0x5413
#define TCGETS 0x5401
#define TCSETS 0x5402

#define BLKGETSIZE64 0x80081272
#define BLKGETSIZE 0x1260
#define BLKSSZGET 0x1268

struct winsize {
  uint16_t ws_row;
  uint16_t ws_col;
  uint16_t ws_xpixel;
  uint16_t ws_ypixel;
};

int ioctl(int fd, unsigned long request, void *arg);
