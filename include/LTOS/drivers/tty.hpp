#pragma once

#include <stddef.h>

namespace tty {

void init();

size_t write(const char *buf, size_t len);

size_t read(char *buf, size_t len);

int ioctl(unsigned long req, void *arg);

} // namespace tty
