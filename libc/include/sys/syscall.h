#pragma once

/*
 * Linux x86_64 compatible syscall numbers
 */

#define SYS_READ 0
#define SYS_WRITE 1
#define SYS_OPEN 2
#define SYS_CLOSE 3
#define SYS_STAT 4
#define SYS_FSTAT 5
#define SYS_LSTAT 6
#define SYS_POLL 7
#define SYS_LSEEK 8
#define SYS_MMAP 9
#define SYS_MPROTECT 10
#define SYS_MUNMAP 11
#define SYS_BRK 12
#define SYS_RT_SIGACTION 13
#define SYS_RT_SIGPROCMASK 14
#define SYS_IOCTL 16
#define SYS_GETTIMEOFDAY 17

#define SYS_ACCESS 21

#define SYS_DUP 32
#define SYS_DUP2 33

#define SYS_NANOSLEEP 35

#define SYS_GETPID 39

#define SYS_CLONE 56
#define SYS_FORK 57
#define SYS_VFORK 58
#define SYS_EXECVE 59
#define SYS_EXIT 60
#define SYS_WAIT4 61

#define SYS_GETCWD 79

#define SYS_CHDIR 80

#define SYS_GETDENTS64 217

/*
 * LosTacOS extensions
 * (Linux programs won't use these)
 */

#define SYS_READDIR 500
#define SYS_OPENDIR 501
#define SYS_CLOSEDIR 502
#define SYS_FSIZE 503
#define SYS_YIELD 504

extern long syscall(long number, ...);
