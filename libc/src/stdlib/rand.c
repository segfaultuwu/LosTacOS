#include <stdint.h>
#include <stdlib.h>

static unsigned long seed = 1;

int rand(void) {
  seed = seed * 1103515245 + 12345;

  return (seed >> 16) & 0x7fff;
}

void srand(unsigned int value) {
  seed = value;
}
