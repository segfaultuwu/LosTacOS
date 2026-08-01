#include <sys/select.h>
#include <sys/syscall.h>

struct pollfd {
  int fd;
  short events;
  short revents;
};

#define POLLIN 0x0001
#define POLLOUT 0x0004
#define POLLERR 0x0008
#define POLLHUP 0x0010
#define POLLNVAL 0x0020

static int popcount32(uint32_t v) {
  int n = 0;
  while (v) {
    n += v & 1;
    v >>= 1;
  }
  return n;
}

int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
           struct timeval *timeout) {
  if (nfds < 0 || nfds > FD_SETSIZE)
    return -1;

  struct pollfd pfds[FD_SETSIZE];
  int slot_for_fd[FD_SETSIZE];
  int n = 0;

  for (int i = 0; i < FD_SETSIZE; i++)
    slot_for_fd[i] = -1;

  for (int fd = 0; fd < nfds; fd++) {
    int want_read = readfds && FD_ISSET(fd, readfds);
    int want_write = writefds && FD_ISSET(fd, writefds);
    int want_except = exceptfds && FD_ISSET(fd, exceptfds);

    if (!want_read && !want_write && !want_except)
      continue;

    short events = 0;
    if (want_read)
      events |= POLLIN;
    if (want_write)
      events |= POLLOUT;

    pfds[n].fd = fd;
    pfds[n].events = events;
    pfds[n].revents = 0;
    slot_for_fd[fd] = n;
    n++;
  }

  int timeout_ms = -1;
  if (timeout)
    timeout_ms = (int)(timeout->tv_sec * 1000 + timeout->tv_usec / 1000);

  long ret = syscall(SYS_POLL, (long)pfds, (long)n, (long)timeout_ms);

  if (ret < 0)
    return -1;

  fd_set out_read, out_write, out_except;
  FD_ZERO(&out_read);
  FD_ZERO(&out_write);
  FD_ZERO(&out_except);

  for (int fd = 0; fd < nfds; fd++) {
    int slot = slot_for_fd[fd];
    if (slot < 0)
      continue;

    short revents = pfds[slot].revents;

    if (readfds && (revents & (POLLIN | POLLHUP | POLLERR | POLLNVAL)))
      FD_SET(fd, &out_read);
    if (writefds && (revents & (POLLOUT | POLLERR | POLLNVAL)))
      FD_SET(fd, &out_write);
    if (exceptfds && (revents & (POLLERR | POLLHUP | POLLNVAL)))
      FD_SET(fd, &out_except);
  }

  if (readfds)
    *readfds = out_read;
  if (writefds)
    *writefds = out_write;
  if (exceptfds)
    *exceptfds = out_except;

  int ready = 0;
  if (readfds)
    ready += popcount32(out_read.bits);
  if (writefds)
    ready += popcount32(out_write.bits);
  if (exceptfds)
    ready += popcount32(out_except.bits);

  return ready;
}
