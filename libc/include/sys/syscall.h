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
#define SYS_FCNTL 72

#define SYS_GETTIMEOFDAY 96

#define SYS_ACCESS 21

/*
 * File descriptor handling
 */

#define SYS_DUP 32
#define SYS_DUP2 33

/*
 * Process
 */

#define SYS_NANOSLEEP 35

#define SYS_GETPID 39

#define SYS_CLONE 56
#define SYS_FORK 57
#define SYS_VFORK 58
#define SYS_EXECVE 59
#define SYS_EXIT 60
#define SYS_WAIT4 61
#define SYS_KILL 62

/*
 * Filesystem
 */

#define SYS_GETCWD 79
#define SYS_CHDIR 80
#define SYS_MKDIR 83
#define SYS_RMDIR 84
#define SYS_LINK 86
#define SYS_UNLINK 87
#define SYS_SYMLINK 88


#define SYS_UNAME 63
#define SYS_TRUNCATE 76
#define SYS_FTRUNCATE 77
#define SYS_SYSINFO 99
#define SYS_GETUID 102
#define SYS_GETGID 104
#define SYS_SETUID 105
#define SYS_SETGID 106
#define SYS_GETEUID 107
#define SYS_GETEGID 108
#define SYS_GETPPID 110

#define SYS_MOUNT 165
#define SYS_UMOUNT2 166



#define SYS_PIPE 22
#define SYS_SCHED_YIELD 24

#define SYS_PAUSE 34
#define SYS_GETITIMER 36
#define SYS_ALARM 37
#define SYS_SETITIMER 38
#define SYS_GETDENTS 78
#define SYS_READLINK 89
#define SYS_CHMOD 90
#define SYS_FCHMOD 91
#define SYS_CHOWN 92
#define SYS_FCHOWN 93
#define SYS_LCHOWN 94
#define SYS_UMASK 95
#define SYS_TIME 201
#define SYS_FUTEX 202
#define SYS_SET_TID_ADDRESS 218
#define SYS_EXIT_GROUP 231
#define SYS_OPENAT 257
#define SYS_MKDIRAT 258
#define SYS_NEWFSTATAT 262
#define SYS_UNLINKAT 263
#define SYS_READLINKAT 267
#define SYS_PIPE2 293

/* LosTacOS custom extensions */
#define SYS_SLEEP 499
#define SYS_READDIR 500
#define SYS_OPENDIR 501
#define SYS_CLOSEDIR 502
#define SYS_FSIZE 503
#define SYS_YIELD 504

extern long syscall(long number, ...);


extern long syscall(long number, ...);

