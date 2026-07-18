#pragma once

#include <stddef.h>

namespace tty {

void init();

void write(const char *buf, size_t len);

size_t read(char *buf, size_t len);

} // namespace tty
