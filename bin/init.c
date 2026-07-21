#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main() {
  char *motd = malloc(100);
  motd = "Hello, LosTacOS!";
  printf("%s\n", motd);
  free(motd);
  exec("/bin/sh");
  return 0;
}
