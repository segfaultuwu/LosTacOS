#pragma once

#include <stdint.h>

#define FBIOGET_SCREENINFO 0x4600

struct fb_screeninfo {
  uint32_t width;
  uint32_t height;
  uint32_t pitch; // bytes per row
  uint32_t bpp;   // bits per pixel
};
