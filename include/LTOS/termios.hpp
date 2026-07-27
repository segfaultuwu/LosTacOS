#pragma once

#include <stdint.h>
#define TCGETS 0x5401
#define TCSETS 0x5402

struct termios {
  uint32_t c_iflag;
  uint32_t c_oflag;
  uint32_t c_cflag;
  uint32_t c_lflag;
  uint8_t c_line;
  uint8_t c_cc[32];
};
