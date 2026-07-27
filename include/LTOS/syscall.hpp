#pragma once

#include <stdint.h>

/*
 * Linux x86_64 syscall ABI
 */

enum Syscall {

  SYS_READ = 0,
  SYS_WRITE = 1,
  SYS_OPEN = 2,
  SYS_CLOSE = 3,

  SYS_LSEEK = 8,

  SYS_MMAP = 9,
  SYS_MUNMAP = 11,

  SYS_IOCTL = 16,

  SYS_GETTIMEOFDAY = 17,

  SYS_GETPID = 39,

  SYS_FORK = 57,
  SYS_EXECVE = 59,
  SYS_EXIT = 60,
  SYS_WAIT4 = 61,

  SYS_NANOSLEEP = 35,

  /*
   * LosTacOS extensions
   * not used by Linux programs
   */
  SYS_SLEEP = 499,
  SYS_READDIR = 500,
  SYS_OPENDIR = 501,
  SYS_CLOSEDIR = 502,
  SYS_FSIZE = 503,
  SYS_YIELD = 504,

};

extern "C" uint64_t syscall_handler(uint64_t num, uint64_t a, uint64_t b, uint64_t c, uint64_t d,
                                    uint64_t e, uint64_t f);
