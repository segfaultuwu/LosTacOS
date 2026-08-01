#pragma once

#include "dirent.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>
#include <termios.h>

typedef long ssize_t;
typedef long off_t;
typedef unsigned int mode_t;

struct timespec {
  long tv_sec;
  long tv_nsec;
};

struct stat {
  uint64_t st_dev;
  uint64_t st_ino;
  uint64_t st_mode;
  uint64_t st_nlink;
  uint64_t st_uid;
  uint64_t st_gid;
  uint64_t st_size;
};

struct linux_dirent64 {
  uint64_t d_ino;
  int64_t d_off;
  unsigned short d_reclen;
  unsigned char d_type;
  char d_name[];
};

#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

int isatty(int fd);

int atexit(void (*func)(void));

/* basic io */

int open(const char *path, int flags, ...);

int close(int fd);

ssize_t read(int fd, void *buf, size_t count);

int sys_readdir(int fd, struct dirent *ent);

ssize_t write(int fd, const void *buf, size_t count);

int pipe(int pipefd[2]);
int dup2(int oldfd, int newfd);

/* process */

void exit(int status);

pid_t getpid(void);

int fork(void);

int execve(const char *pathname, char *const argv[], char *const envp[]);

pid_t wait4(pid_t pid, int *status, int options, void *rusage);

/* scheduler */

int sched_yield(void);

/* time */

int nanosleep(const struct timespec *req, struct timespec *rem);

unsigned int sleep(unsigned int seconds);

/* files */

off_t lseek(int fd, off_t offset, int whence);

int unlink(const char *path);

int mkdir(const char *path, mode_t mode);

int rmdir(const char *path);

int chdir(const char *path);

char *getcwd(char *buf, size_t size);

/* memory */

void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);

int munmap(void *addr, size_t length);

int brk(void *addr);

/* directory */

int getdents64(unsigned int fd, struct linux_dirent64 *dirp, unsigned int count);

/* terminal */

int isatty(int fd);

/* misc */

int dup(int oldfd);

int dup2(int oldfd, int newfd);

int stat(const char *path, struct stat *buf);

long syscall(long number, ...);
