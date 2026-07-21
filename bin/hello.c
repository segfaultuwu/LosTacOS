#include <stdio.h>
#include <sys/syscall.h>

int main() {
  char name[64];
  int age;

  puts("Name:");

  scanf("%s", name);

  puts("Age:");

  scanf("%d", &age);

  printf("Hello %s age %d\n", name, age);

  return 0;
}
