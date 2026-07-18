#pragma once

#include <stddef.h>

namespace console {

void init();

void write(const char *buf, size_t len);

void put(char c);

} // namespace console
