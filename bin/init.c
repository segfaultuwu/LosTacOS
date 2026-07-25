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

  printf("Uptime: %s", buf);

  int fd2 = open("/proc/version");
  if (fd2 < 0) {
    printf("open(\"/proc/version\") failed\n");
  }

  char buf2[256];
  long n2 = read(fd2, buf2, sizeof(buf2));

  printf("Version: %s", buf2);

  exec("/usr/bin/sh");
  return 0;
}
