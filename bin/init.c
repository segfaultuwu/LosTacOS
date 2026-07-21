#include <unistd.h>

int main() {
  exec("/bin/sh");
  return 0;
}
