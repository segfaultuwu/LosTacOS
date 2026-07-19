#pragma once

#include "LTOS/drivers/keyboard/keycodes.hpp"
#include <stdint.h>

namespace drivers::keyboard {

using namespace drivers::keyboard::keycodes;

KeyEvent get_event();
char event_to_ascii(KeyEvent event);

char getchar();
char *getstring();

} // namespace drivers::keyboard
