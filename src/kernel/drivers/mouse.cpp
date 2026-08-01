#include "LTOS/drivers/mouse.hpp"
#include "LTOS/drivers/framebuffer.hpp"
#include "LTOS/drivers/pic.hpp"
#include "LTOS/drivers/serial.hpp"
#include "LTOS/fs/devfs.hpp"
#include <string.h>

namespace drivers::mouse {

static int mouse_x = 512;
static int mouse_y = 384;
static bool btn_left = false;
static bool btn_right = false;
static bool btn_middle = false;

static uint8_t mouse_cycle = 0;
static uint8_t mouse_bytes[3];

// Circular buffer for /dev/psaux (standard 3-byte mouse packets)
static uint8_t psaux_ring[256];
static size_t ring_head = 0;
static size_t ring_tail = 0;

static void outb(uint16_t port, uint8_t val) {
  asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static uint8_t inb(uint16_t port) {
  uint8_t ret;
  asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
  return ret;
}

static void wait_input_empty() {
  uint32_t timeout = 100000;
  while (timeout--) {
    if ((inb(0x64) & 2) == 0)
      return;
  }
}

static void wait_output_full() {
  uint32_t timeout = 100000;
  while (timeout--) {
    if ((inb(0x64) & 1) == 1)
      return;
  }
}

static void mouse_wait_aux() {
  uint32_t timeout = 100000;
  while (timeout--) {
    uint8_t stat = inb(0x64);
    if ((stat & 0x21) == 0x21)
      return;
  }
}

static void mouse_write(uint8_t write) {
  wait_input_empty();
  outb(0x64, 0xD4);
  wait_input_empty();
  outb(0x60, write);
}

static uint8_t mouse_read() {
  mouse_wait_aux();
  if ((inb(0x64) & 0x21) == 0x21)
    return inb(0x60);
  return 0;
}

void init() {
  drivers::serial::write("PS2 Mouse: Initializing...\n");

  uint32_t fb_w = framebuffer::get_width();
  uint32_t fb_h = framebuffer::get_height();
  if (fb_w > 0 && fb_h > 0) {
    mouse_x = fb_w / 2;
    mouse_y = fb_h / 2;
  }

  // Enable aux device (0xA8)
  wait_input_empty();
  outb(0x64, 0xA8);

  // Read 8042 Controller Command Byte (0x20)
  wait_input_empty();
  outb(0x64, 0x20);
  wait_output_full();
  uint8_t status = inb(0x60);

  // Enable Keyboard IRQ 1 (bit 0) & Mouse IRQ 12 (bit 1)
  // Clear Disable Keyboard (bit 4) & Disable Mouse (bit 5)
  status |= 0x01 | 0x02;
  status &= ~(0x10 | 0x20);

  // Write 8042 Controller Command Byte (0x60)
  wait_input_empty();
  outb(0x64, 0x60);
  wait_input_empty();
  outb(0x60, status);

  // Default settings
  mouse_write(0xF6);
  mouse_read();

  // Enable packet streaming
  mouse_write(0xF4);
  mouse_read();

  drivers::serial::write("PS2 Mouse: Initialized successfully.\n");
}

void irq_handler() {
  uint8_t status = inb(0x64);
  if (!(status & 1) || !(status & 0x20))
    return;

  uint8_t b = inb(0x60);

  switch (mouse_cycle) {
  case 0:
    if ((b & 0x08) == 0x08) { // Bit 3 must be 1 in 3-byte PS/2 mode
      mouse_bytes[0] = b;
      mouse_cycle++;
    }
    break;
  case 1:
    mouse_bytes[1] = b;
    mouse_cycle++;
    break;
  case 2: {
    mouse_bytes[2] = b;
    mouse_cycle = 0;

    uint8_t status_byte = mouse_bytes[0];

    btn_left = (status_byte & 0x01) != 0;
    btn_right = (status_byte & 0x02) != 0;
    btn_middle = (status_byte & 0x04) != 0;

    // dx/dy are 9-bit two's-complement values: 8 data bits plus an
    // explicit sign bit in the status byte. A naive int8_t cast of the
    // data byte only agrees with this for |delta| < 128 — faster
    // movements need the real sign bit, or the cursor jumps the wrong
    // direction.
    int rel_x = mouse_bytes[1];
    if (status_byte & 0x10) // X sign bit
      rel_x -= 256;

    int rel_y = mouse_bytes[2];
    if (status_byte & 0x20) // Y sign bit
      rel_y -= 256;

    // X/Y overflow bits: packet data is unreliable, don't apply it.
    bool overflow = (status_byte & 0xC0) != 0;

    if (!overflow) {
      mouse_x += rel_x;
      mouse_y -= rel_y; // PS/2 Y delta is inverted
    }

    uint32_t max_x = framebuffer::get_width() > 0 ? framebuffer::get_width() - 1 : 1023;
    uint32_t max_y = framebuffer::get_height() > 0 ? framebuffer::get_height() - 1 : 767;

    if (mouse_x < 0)
      mouse_x = 0;
    if ((uint32_t)mouse_x > max_x)
      mouse_x = max_x;
    if (mouse_y < 0)
      mouse_y = 0;
    if ((uint32_t)mouse_y > max_y)
      mouse_y = max_y;

    // Push 3-byte packet into /dev/psaux ring buffer
    for (int i = 0; i < 3; i++) {
      psaux_ring[ring_head] = mouse_bytes[i];
      ring_head = (ring_head + 1) % sizeof(psaux_ring);
      if (ring_head == ring_tail)
        ring_tail = (ring_tail + 1) % sizeof(psaux_ring);
    }
    break;
  }
  }
}

MouseState get_state() {
  return {.x = mouse_x, .y = mouse_y, .left = btn_left, .right = btn_right, .middle = btn_middle};
}

/*
 * /dev/psaux device ops
 */
static size_t psaux_read(char *buf, size_t len, size_t offset) {
  (void)offset;
  size_t n = 0;
  while (n < len && ring_tail != ring_head) {
    buf[n++] = (char)psaux_ring[ring_tail];
    ring_tail = (ring_tail + 1) % sizeof(psaux_ring);
  }
  return n;
}

static size_t psaux_write(const char *buf, size_t len, size_t offset) {
  (void)buf;
  (void)offset;
  return len;
}

static fs::vfs::DevOps psaux_ops = {.write = psaux_write, .read = psaux_read, .ioctl = nullptr};

void register_dev() {
  fs::devfs::register_device("psaux", &psaux_ops);
}

} // namespace drivers::mouse

extern "C" sched::Registers *mouse_irq(sched::Registers *regs) {
  drivers::mouse::irq_handler();
  drivers::pic::eoi(12);
  return regs;
}
