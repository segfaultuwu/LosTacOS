#pragma once

#include <stdint.h>

#define TIOCGWINSZ 0x5413
#define TCGETS 0x5401
#define TCSETS 0x5402

struct winsize {
  uint16_t ws_row;
  uint16_t ws_col;
  uint16_t ws_xpixel;
  uint16_t ws_ypixel;
};

struct termios {
  uint32_t c_iflag;
  uint32_t c_oflag;
  uint32_t c_cflag;
  uint32_t c_lflag;

  uint8_t c_line;

  uint8_t c_cc[32];
};
