#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

int tcgetattr(int fd, struct termios *t) {
  if (!t)
    return -1;
  return ioctl(fd, TCGETS, t);
}

int tcsetattr(int fd, int optional_actions, const struct termios *t) {
  if (!t)
    return -1;

  int req = TCSETS;
  if (optional_actions == TCSADRAIN || optional_actions == TCSETSW) {
    req = TCSETSW;
  } else if (optional_actions == TCSAFLUSH || optional_actions == TCSETSF) {
    req = TCSETSF;
  }

  return ioctl(fd, req, (void *)t);
}

void cfmakeraw(struct termios *t) {
  if (!t)
    return;
  t->c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
  t->c_oflag &= ~OPOST;
  t->c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
  t->c_cflag &= ~(CSIZE | PARENB);
  t->c_cflag |= CS8;
}

speed_t cfgetispeed(const struct termios *t) {
  return t ? t->c_ispeed : 0;
}

speed_t cfgetospeed(const struct termios *t) {
  return t ? t->c_ospeed : 0;
}

int cfsetispeed(struct termios *t, speed_t speed) {
  if (!t)
    return -1;
  t->c_ispeed = speed;
  return 0;
}

int cfsetospeed(struct termios *t, speed_t speed) {
  if (!t)
    return -1;
  t->c_ospeed = speed;
  return 0;
}
