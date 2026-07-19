#include "LTOS/drivers/keyboard.hpp"
#include "LTOS/drivers/console.hpp"
#include "LTOS/drivers/serial.hpp"
#include <cstdint>

namespace drivers::keyboard {

constexpr uint16_t KBD_DATA = 0x60;
constexpr uint16_t KBD_STATUS = 0x64;

constexpr uint8_t STATUS_OUTPUT_FULL = 0x01;

static bool shift = false;
static bool caps = false;
static bool ctrl = false;
static bool alt = false;

constexpr size_t QUEUE_SIZE = 128;

static KeyEvent queue[QUEUE_SIZE];

static volatile uint32_t queue_read = 0;
static volatile uint32_t queue_write = 0;

// Set 1 scancodes for keys that arrive as a plain byte (no 0xE0 lead byte).
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

  case 0x1D:
    return KEY_LEFT_CTRL;
  case 0x38:
    return KEY_LEFT_ALT;

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

  case 0x0C:
    return KEY_MINUS;
  case 0x0D:
    return KEY_EQUAL;
  case 0x1A:
    return KEY_LEFT_BRACKET;
  case 0x1B:
    return KEY_RIGHT_BRACKET;
  case 0x27:
    return KEY_SEMICOLON;
  case 0x28:
    return KEY_APOSTROPHE;
  case 0x29:
    return KEY_GRAVE;
  case 0x2B:
    return KEY_BACKSLASH;
  case 0x33:
    return KEY_COMMA;
  case 0x34:
    return KEY_DOT;
  case 0x35:
    return KEY_SLASH;

  case 0x3B:
    return KEY_F1;
  case 0x3C:
    return KEY_F2;
  case 0x3D:
    return KEY_F3;
  case 0x3E:
    return KEY_F4;
  case 0x3F:
    return KEY_F5;
  case 0x40:
    return KEY_F6;
  case 0x41:
    return KEY_F7;
  case 0x42:
    return KEY_F8;
  case 0x43:
    return KEY_F9;
  case 0x44:
    return KEY_F10;
  case 0x57:
    return KEY_F11;
  case 0x58:
    return KEY_F12;

  case 0x45:
    return KEY_NUM_LOCK;
  case 0x46:
    return KEY_SCROLL_LOCK;

  // Base (non-extended) numpad matrix. These scancodes are shared with
  // Insert/Delete/Home/End/PageUp/PageDown/arrows -- the *extended*
  // (0xE0-prefixed) versions of those same byte values mean the
  // navigation key instead, handled separately in
  // extended_scancode_to_key() below.
  case 0x37:
    return KEY_KP_MULTIPLY;
  case 0x4A:
    return KEY_KP_MINUS;
  case 0x4E:
    return KEY_KP_PLUS;
  case 0x47:
    return KEY_KP_7;
  case 0x48:
    return KEY_KP_8;
  case 0x49:
    return KEY_KP_9;
  case 0x4B:
    return KEY_KP_4;
  case 0x4C:
    return KEY_KP_5;
  case 0x4D:
    return KEY_KP_6;
  case 0x4F:
    return KEY_KP_1;
  case 0x50:
    return KEY_KP_2;
  case 0x51:
    return KEY_KP_3;
  case 0x52:
    return KEY_KP_0;
  case 0x53:
    return KEY_KP_DOT;

  default:
    return KEY_NONE;
  }
}

// Set 1 scancodes that arrive prefixed with a lead byte of 0xE0.
static KeyCode extended_scancode_to_key(uint8_t sc) {

  switch (sc) {

  case 0x1C:
    return KEY_KP_ENTER;
  case 0x35:
    return KEY_KP_DIVIDE;

  case 0x1D:
    return KEY_RIGHT_CTRL;
  case 0x38:
    return KEY_RIGHT_ALT;

  case 0x48:
    return KEY_ARROW_UP;
  case 0x50:
    return KEY_ARROW_DOWN;
  case 0x4B:
    return KEY_ARROW_LEFT;
  case 0x4D:
    return KEY_ARROW_RIGHT;

  case 0x52:
    return KEY_INSERT;
  case 0x53:
    return KEY_DELETE;
  case 0x47:
    return KEY_HOME;
  case 0x4F:
    return KEY_END;
  case 0x49:
    return KEY_PAGE_UP;
  case 0x51:
    return KEY_PAGE_DOWN;

  default:
    // Print Screen and Pause use their own multi-byte, non-uniform
    // sequences (Print Screen: E0 2A E0 37 on press; Pause: E1 1D 45 E1
    // 9D C5, no distinct release code at all). Neither is handled here --
    // not needed for shell use, and Pause in particular doesn't fit this
    // press/release model at all.
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

  if (key >= KEY_1 && key <= KEY_9) {

    static const char nums[] = "123456789";
    static const char shifted[] = "!@#$%^&*(";

    char c = nums[key - KEY_1];

    if (shift)
      c = shifted[key - KEY_1];

    return c;
  }

  if (key == KEY_0)
    return shift ? ')' : '0';

  switch (key) {

  case KEY_SPACE:
    return ' ';

  case KEY_ENTER:
  case KEY_KP_ENTER:
    return '\n';

  case KEY_BACKSPACE:
    return '\b';

  case KEY_TAB:
    return '\t';

  // Symbols. Shift-combos here are what actually let the shell's
  // pipe/redirect syntax get typed at all: shift+backslash for '|',
  // shift+comma for '<', shift+dot for '>'.
  case KEY_MINUS:
    return shift ? '_' : '-';
  case KEY_EQUAL:
    return shift ? '+' : '=';
  case KEY_LEFT_BRACKET:
    return shift ? '{' : '[';
  case KEY_RIGHT_BRACKET:
    return shift ? '}' : ']';
  case KEY_SEMICOLON:
    return shift ? ':' : ';';
  case KEY_APOSTROPHE:
    return shift ? '"' : '\'';
  case KEY_GRAVE:
    return shift ? '~' : '`';
  case KEY_BACKSLASH:
    return shift ? '|' : '\\';
  case KEY_COMMA:
    return shift ? '<' : ',';
  case KEY_DOT:
    return shift ? '>' : '.';
  case KEY_SLASH:
    return shift ? '?' : '/';

  // Numpad, treated as always-numeric (no Num Lock state tracking yet --
  // the arrow/nav dual-meaning of these physical keys is handled entirely
  // in extended_scancode_to_key() via the 0xE0 prefix instead).
  case KEY_KP_0:
    return '0';
  case KEY_KP_1:
    return '1';
  case KEY_KP_2:
    return '2';
  case KEY_KP_3:
    return '3';
  case KEY_KP_4:
    return '4';
  case KEY_KP_5:
    return '5';
  case KEY_KP_6:
    return '6';
  case KEY_KP_7:
    return '7';
  case KEY_KP_8:
    return '8';
  case KEY_KP_9:
    return '9';
  case KEY_KP_DOT:
    return '.';
  case KEY_KP_PLUS:
    return '+';
  case KEY_KP_MINUS:
    return '-';
  case KEY_KP_MULTIPLY:
    return '*';
  case KEY_KP_DIVIDE:
    return '/';

  default:
    return 0;
  }
}

KeyEvent get_event() {
  while (queue_empty())
    ;

  return pop();
}

static void push_event(KeyEvent e) {
  uint32_t next = (queue_write + 1) % QUEUE_SIZE;

  if (next == queue_read)
    return; // full

  queue[queue_write] = e;
  queue_write = next;
}

bool queue_empty() {
  return queue_read == queue_write;
}

KeyEvent pop() {
  if (queue_empty())
    return {.key = KEY_NONE, .pressed = false};

  KeyEvent e = queue[queue_read];

  queue_read = (queue_read + 1) % QUEUE_SIZE;

  return e;
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

void irq_handler() {
  uint8_t sc = drivers::serial::inb(KBD_DATA);

  // 0xE0 is a lead byte, not a key itself: it means "the next byte is one
  // of the extended keys" (arrows, Insert/Delete/Home/End/PageUp/PageDown,
  // right Ctrl/Alt, numpad Enter/Divide). It carries no press/release bit
  // of its own, so just remember it and wait for the following byte.
  //
  // 0xE1 (Pause's lead byte) is deliberately swallowed here too: Pause's
  // sequence doesn't fit this model at all (six bytes, no release code),
  // so the best this driver can do without properly parsing it is avoid
  // letting it desync `extended` for the *next* real key.
  static bool extended = false;

  if (sc == 0xE0) {
    extended = true;
    return;
  }

  if (sc == 0xE1) {
    return;
  }

  bool is_extended = extended;
  extended = false;

  bool released = sc & 0x80;
  sc &= 0x7f;

  KeyCode key = is_extended ? extended_scancode_to_key(sc) : scancode_to_key(sc);

  if (released) {
    if (key == KEY_LEFT_SHIFT || key == KEY_RIGHT_SHIFT)
      shift = false;

    if (key == KEY_LEFT_CTRL || key == KEY_RIGHT_CTRL)
      ctrl = false;

    if (key == KEY_LEFT_ALT || key == KEY_RIGHT_ALT)
      alt = false;

    push_event(
        {.key = key, .pressed = false, .shift = shift, .ctrl = ctrl, .alt = alt, .scancode = sc});

    return;
  }

  if (key == KEY_LEFT_SHIFT || key == KEY_RIGHT_SHIFT)
    shift = true;

  if (key == KEY_LEFT_CTRL || key == KEY_RIGHT_CTRL)
    ctrl = true;

  if (key == KEY_LEFT_ALT || key == KEY_RIGHT_ALT)
    alt = true;

  if (key == KEY_CAPS_LOCK)
    caps = !caps;

  push_event(
      {.key = key, .pressed = true, .shift = shift, .ctrl = ctrl, .alt = alt, .scancode = sc});
}

} // namespace drivers::keyboard
