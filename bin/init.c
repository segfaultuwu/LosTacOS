#include <stdio.h>
#include <unistd.h>

int main() {
  char *motd = "Hello, LosTacOS!";
  printf("%s\n", motd);

  int fd = open("/proc/uptime_ms");
  if (fd < 0) {
    printf("open(\"/proc/uptime_ms\") failed\n");
  } else {
    char buf[256];
    long n = read(fd, buf, sizeof(buf) - 1);
    if (n < 0)
      n = 0;
    buf[n] = '\0';

    printf("Boot time: %sms\n", buf);
    close(fd);
  }

  int fd2 = open("/proc/version");
  if (fd2 < 0) {
    printf("open(\"/proc/version\") failed\n");
  } else {
    char buf2[256];
    long n2 = read(fd2, buf2, sizeof(buf2) - 1);
    if (n2 < 0)
      n2 = 0;
    buf2[n2] = '\0';

    printf("Version: %s\n", buf2);
    close(fd2);
  }

  printf("Launching /bin/sh..\n");

  char *argv[] = {"sh", NULL};

  char *envp[] = {NULL};

  execve("/bin/sh", argv, envp);

  printf("execve(\"/bin/sh\") failed\n");
  return 1;
}
