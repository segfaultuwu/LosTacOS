#pragma once

#include "LTOS/drivers/psf.hpp"
#include <stddef.h>
#include <stdint.h>

namespace console {

void init();

void write(const char *buf, size_t len);

void put(char c);
void put_swap(char c);

void set_font(psf::Font *f);

void newline();

void backspace();

void clear();

void cursor_tick();

uint32_t get_rows();
uint32_t get_cols();

void lock();
void unlock();

} // namespace console
