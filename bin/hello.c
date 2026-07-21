#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

#define MAX_CMD 128

static void execute(char *cmd) {
  if (strlen(cmd) == 0)
    return;

  if (strcmp(cmd, "exit") == 0) {
    exit(0);
  }

  int pid = fork();

  if (pid == 0) {
    exec(cmd);

    printf("sh: command not found: %s\n", cmd);

    exit(1);
  }

  // parent
  wait(pid);
}

int main() {
  char cmd[MAX_CMD];

  printf("LosTacOS shell\n");

  while (1) {

    printf("los> ");

    scanf("%127s", cmd);

    execute(cmd);
  }

  return 0;
}
