#pragma once

#include <stdint.h>

namespace vga
{
    void clear();
    void put(char c);
    void write(const char* str);
}
