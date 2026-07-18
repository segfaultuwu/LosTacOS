#pragma once

#include "LTOS/drivers/psf.hpp"
#include <stddef.h>

namespace console {

void init();

void write(const char *buf, size_t len);

void put(char c);

void set_font(psf::Font *f);

} // namespace console
