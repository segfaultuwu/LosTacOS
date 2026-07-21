#pragma once

#define SYS_WRITE 1
#define SYS_READ 2
#define SYS_EXIT 3
#define SYS_OPEN 4
#define SYS_CLOSE 5
#define SYS_FORK 6
#define SYS_EXEC 7
#define SYS_GETPID 8
#define SYS_YIELD 9
#define SYS_SLEEP 10
#define SYS_LSEEK 11
#define SYS_FSIZE 12
#define SYS_WAIT 13

extern long syscall(long num, long a, long b, long c);
