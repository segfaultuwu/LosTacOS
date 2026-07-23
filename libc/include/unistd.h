#pragma once

#include <stddef.h>
#include <stdint.h>

/*
 * File offsets
 */

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

/*
 * Process
 */

extern int getpid(void);

extern int yield(void);

extern int fork(void);

extern long wait(int pid);

extern void exit(int status);

extern int exec(const char *path);

/*
 * Time
 */

extern void sleep_ms(unsigned long ms);

/*
 * Files
 */

extern int open(const char *path);

extern int close(int fd);

extern long read(int fd, void *buf, size_t len);

extern long write(int fd, const void *buf, size_t len);

extern long lseek(int fd, long offset, int whence);

extern long fsize(int fd);

/*
 * Memory
 */

extern void *malloc(size_t size);

extern void free(void *ptr);

/*
 * Console helpers
 */

extern int getchar(void);

extern int putchar(int c);

/*
 * System
 */

extern long syscall(long number, long arg1, long arg2, long arg3);
