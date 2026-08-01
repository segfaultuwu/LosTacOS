#include <stdio.h>
#include <unistd.h>

int main() {
  char *motd = "Hello, LosTacOS!";
  printf("%s\n", motd);

  int fd = open("/proc/uptime", 0);
  if (fd < 0) {
    printf("open(\"/proc/uptime\") failed\n");
  } else {
    char buf[256];
    long n = read(fd, buf, sizeof(buf) - 1);
    if (n < 0)
      n = 0;
    buf[n] = '\0';

    printf("Boot time: %s\n", buf);
    close(fd);
  }

  int fd2 = open("/proc/version", 0);
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

  printf("Launching /bin/sh..\n\n");

  char *argv[] = {"sh", NULL};
  char *envp[] = {NULL};

  int pid = fork();

  if (pid < 0) {
    printf("fork() failed\n");
    return 1;
  }

  if (pid == 0) {
    execve("/bin/sh", argv, envp);
    printf("execve(\"/bin/sh\") failed\n");
    exit(127);
  }

  while (1) {
    int status = 0;
    wait4(pid, &status, 0, NULL);

    pid = fork();
    if (pid < 0)
      break;

    if (pid == 0) {
      execve("/bin/sh", argv, envp);
      printf("execve(\"/bin/sh\") failed\n");
      exit(127);
    }
  }

  return 0;
}
