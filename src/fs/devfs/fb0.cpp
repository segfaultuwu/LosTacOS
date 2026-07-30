#include "LTOS/drivers/framebuffer.hpp"
#include "LTOS/fs/devfs.hpp"

#include <string.h>
#include <sys/fb.h>

namespace fs::devfs {

static size_t fb_size() {
  return framebuffer::get_pitch() * framebuffer::get_height();
}

static size_t fb_write(const char *buf, size_t len, size_t offset) {
  uint8_t *back = framebuffer::get_backbuffer();

  if (!back)
    return 0;

  size_t size = fb_size();

  if (offset >= size)
    return 0;

  size_t remaining = size - offset;

  if (len > remaining)
    len = remaining;

  memcpy(back + offset, buf, len);

  framebuffer::swap();

  return len;
}

static size_t fb_read(char *buf, size_t len, size_t offset) {
  uint8_t *back = framebuffer::get_backbuffer();

  if (!back)
    return 0;

  size_t size = fb_size();

  if (offset >= size)
    return 0;

  size_t remaining = size - offset;

  if (len > remaining)
    len = remaining;

  memcpy(buf, back + offset, len);

  return len;
}

static int fb_ioctl(unsigned long req, void *arg) {
  if (req == FBIOGET_SCREENINFO) {
    auto *info = (fb_screeninfo *)arg;

    if (!info)
      return -1;

    info->width = framebuffer::get_width();
    info->height = framebuffer::get_height();
    info->pitch = framebuffer::get_pitch();
    info->bpp = framebuffer::get_bpp();

    return 0;
  }

  return -1;
}

static vfs::DevOps fb_ops = {.write = fb_write, .read = fb_read, .ioctl = fb_ioctl};

void init_fb() {
  register_device("fb0", &fb_ops);
}

} // namespace fs::devfs
