#include <stdio.h>
#include <unistd.h>

int main() {
  char *motd = "Hello, LosTacOS!";
  printf("%s\n", motd);

  int fd = open("/proc/uptime");
  if (fd < 0) {
    printf("open(\"/proc/uptime\") failed\n");
  }

  char buf[256];
  long n = read(fd, buf, sizeof(buf));

  write(1, buf, n);

  exec("/usr/bin/sh");
  return 0;
}
