extern long syscall(long num, long a, long b, long c);

#define SYS_EXIT 3

void exit(int code) {
  syscall(SYS_EXIT, code, 0, 0);

  while (1)
    ;
}
