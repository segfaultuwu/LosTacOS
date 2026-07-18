#include "LTOS/drivers/keyboard.hpp"
#include "LTOS/console.hpp"
#include "LTOS/drivers/serial.hpp"
#include <stdint.h>
#include <string.h>

namespace drivers::keyboard {

constexpr uint16_t KBD_DATA = 0x60;
constexpr uint16_t KBD_STATUS = 0x64;

constexpr uint8_t STATUS_OUTPUT_FULL = 0x01;

static const char scancode_table[128] = {
    0,    27,

    '1',  '2',  '3',  '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',

    '\t',

    'q',  'w',  'e',  'r', 't', 'y', 'u', 'i', 'o', 'p',

    '[',  ']',  '\n',

    0,

    'a',  's',  'd',  'f', 'g', 'h', 'j', 'k', 'l',

    ';',  '\'', '`',

    0,

    '\\',

    'z',  'x',  'c',  'v', 'b', 'n', 'm',

    ',',  '.',  '/',

    0,

    '*',

    0,

    ' '};

static bool shift = false;
static bool caps = false;

static char apply_shift(char c) {
  if (c >= 'a' && c <= 'z')
    return c - 32;

  switch (c) {
  case '1':
    return '!';
  case '2':
    return '@';
  case '3':
    return '#';
  case '4':
    return '$';
  case '5':
    return '%';
  case '6':
    return '^';
  case '7':
    return '&';
  case '8':
    return '*';
  case '9':
    return '(';
  case '0':
    return ')';

  case '-':
    return '_';
  case '=':
    return '+';

  case '[':
    return '{';
  case ']':
    return '}';

  case ';':
    return ':';
  case '\'':
    return '"';

  case ',':
    return '<';
  case '.':
    return '>';

  case '/':
    return '?';
  }

  return c;
}

char getchar() {
  bool extended = false;

  while (true) {
    if (!(drivers::serial::inb(KBD_STATUS) & STATUS_OUTPUT_FULL))
      continue;

    uint8_t scancode = drivers::serial::inb(KBD_DATA);

    if (scancode == 0xE0) {
      extended = true;
      continue;
    }

    bool released = scancode & 0x80;

    if (released) {
      scancode &= 0x7F;

      if (scancode == 0x2A || scancode == 0x36) {
        shift = false;
      }

      extended = false;
      continue;
    }

    switch (scancode) {
    case 0x2A:
    case 0x36:
      shift = true;
      continue;

    case 0x3A:
      caps = !caps;
      continue;
    }

    if (extended) {
      extended = false;
      continue;
    }

    char c = scancode_table[scancode];

    if (!c)
      continue;

    if (c >= 'a' && c <= 'z') {
      if (shift ^ caps)
        c = apply_shift(c);
    } else if (shift) {
      c = apply_shift(c);
    }

    console::put(c);

    return c;
  }
}

char *getstring() {
  static char buffer[256];

  size_t index = 0;

  while (index < 255) {
    char c = drivers::keyboard::getchar();
    if (c == '\n') {
      buffer[index] = '\0';
      break;
    }

    if (c == '\b') {
      if (index > 0) {
        index--;
        buffer[index] = '\0';
      }

      continue;
    }

    buffer[index++] = c;
  }

  buffer[index] = '\0';

  return buffer;
}
} // namespace drivers::keyboard
