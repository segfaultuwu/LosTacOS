#include "LTOS/drivers/keyboard.hpp"
#include "LTOS/drivers/console.hpp"
#include "LTOS/drivers/serial.hpp"

namespace drivers::keyboard {

constexpr uint16_t KBD_DATA = 0x60;
constexpr uint16_t KBD_STATUS = 0x64;

constexpr uint8_t STATUS_OUTPUT_FULL = 0x01;

static bool shift = false;
static bool caps = false;

static KeyCode scancode_to_key(uint8_t sc) {

  switch (sc) {

  case 0x1C:
    return KEY_ENTER;
  case 0x01:
    return KEY_ESCAPE;
  case 0x0E:
    return KEY_BACKSPACE;
  case 0x0F:
    return KEY_TAB;
  case 0x39:
    return KEY_SPACE;

  case 0x2A:
    return KEY_LEFT_SHIFT;
  case 0x36:
    return KEY_RIGHT_SHIFT;

  case 0x3A:
    return KEY_CAPS_LOCK;

  case 0x1E:
    return KEY_A;
  case 0x30:
    return KEY_B;
  case 0x2E:
    return KEY_C;
  case 0x20:
    return KEY_D;
  case 0x12:
    return KEY_E;
  case 0x21:
    return KEY_F;
  case 0x22:
    return KEY_G;
  case 0x23:
    return KEY_H;
  case 0x17:
    return KEY_I;
  case 0x24:
    return KEY_J;
  case 0x25:
    return KEY_K;
  case 0x26:
    return KEY_L;
  case 0x32:
    return KEY_M;
  case 0x31:
    return KEY_N;
  case 0x18:
    return KEY_O;
  case 0x19:
    return KEY_P;
  case 0x10:
    return KEY_Q;
  case 0x13:
    return KEY_R;
  case 0x1F:
    return KEY_S;
  case 0x14:
    return KEY_T;
  case 0x16:
    return KEY_U;
  case 0x2F:
    return KEY_V;
  case 0x11:
    return KEY_W;
  case 0x2D:
    return KEY_X;
  case 0x15:
    return KEY_Y;
  case 0x2C:
    return KEY_Z;

  case 0x02:
    return KEY_1;
  case 0x03:
    return KEY_2;
  case 0x04:
    return KEY_3;
  case 0x05:
    return KEY_4;
  case 0x06:
    return KEY_5;
  case 0x07:
    return KEY_6;
  case 0x08:
    return KEY_7;
  case 0x09:
    return KEY_8;
  case 0x0A:
    return KEY_9;
  case 0x0B:
    return KEY_0;

  default:
    return KEY_NONE;
  }
}

char key_to_ascii(KeyCode key) {

  if (key >= KEY_A && key <= KEY_Z) {

    char c = 'a' + (key - KEY_A);

    if (shift ^ caps)
      c -= 32;

    return c;
  }

  if (key >= KEY_0 && key <= KEY_9) {

    static const char nums[] = "1234567890";

    char c = nums[key - KEY_1];

    if (shift) {

      static const char shifted[] = "!@#$%^&*()";
      c = shifted[key - KEY_1];
    }

    return c;
  }

  switch (key) {

  case KEY_SPACE:
    return ' ';

  case KEY_ENTER:
    return '\n';

  case KEY_BACKSPACE:
    return '\b';

  case KEY_TAB:
    return '\t';

  default:
    return 0;
  }
}

KeyEvent get_event() {

  while (!(drivers::serial::inb(KBD_STATUS) & STATUS_OUTPUT_FULL))
    ;

  uint8_t sc = drivers::serial::inb(KBD_DATA);

  bool released = sc & 0x80;

  if (released) {

    sc &= 0x7f;

    if (sc == 0x2A || sc == 0x36)
      shift = false;

    return {.key = scancode_to_key(sc),
            .pressed = false,
            .shift = shift,
            .ctrl = false,
            .alt = false,
            .scancode = sc};
  }

  KeyCode key = scancode_to_key(sc);

  if (key == KEY_LEFT_SHIFT || key == KEY_RIGHT_SHIFT)
    shift = true;

  if (key == KEY_CAPS_LOCK)
    caps = !caps;

  return {.key = key,
          .pressed = true,
          .shift = shift,
          .ctrl = false,
          .alt = false,
          .scancode = sc};
}

char getchar() {

  while (true) {

    auto e = get_event();

    if (!e.pressed)
      continue;

    char c = key_to_ascii(e.key);

    if (!c)
      continue;

    console::put_swap(c);

    return c;
  }
}

char *getstring() {

  static char buffer[256];

  size_t index = 0;

  while (index < 255) {

    char c = getchar();

    if (c == '\n') {

      buffer[index] = 0;
      break;
    }

    if (c == '\b') {

      if (index)
        index--;

      continue;
    }

    buffer[index++] = c;
  }

  buffer[index] = 0;

  return buffer;
}

} // namespace drivers::keyboard
