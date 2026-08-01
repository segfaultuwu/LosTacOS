#pragma once

#include <stdint.h>
#include <sys/time.h>

#define FD_SETSIZE 32

typedef struct {
  uint32_t bits;
} fd_set;

#define FD_ZERO(set) ((set)->bits = 0)
#define FD_SET(fd, set) do { if ((fd) >= 0 && (fd) < FD_SETSIZE) (set)->bits |= (1u << (fd)); } while(0)
#define FD_CLR(fd, set) do { if ((fd) >= 0 && (fd) < FD_SETSIZE) (set)->bits &= ~(1u << (fd)); } while(0)
#define FD_ISSET(fd, set) (((fd) >= 0 && (fd) < FD_SETSIZE) ? (((set)->bits >> (fd)) & 1u) : 0)

#ifdef __cplusplus
extern "C" {
#endif

int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);

#ifdef __cplusplus
}
#endif
