#pragma once

#include <stddef.h>

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
extern void exit(int pid);

extern int exec(const char *path);

/*
 * Time
 *
 * Note: unlike POSIX sleep(), this takes milliseconds, not seconds --
 * it maps directly onto the kernel's timer::sleep(ms).
 */

extern void sleep_ms(unsigned long ms);

/*
 * Files
 */

extern long lseek(int fd, long offset, int whence);

extern long fsize(int fd);
