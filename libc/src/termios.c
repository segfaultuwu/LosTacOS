#include <sys/ioctl.h>
#include <termios.h>

int tcgetattr(int fd, struct termios *term) {
  return ioctl(fd, TCGETS, term);
}

int tcsetattr(int fd, int optional_actions, const struct termios *term) {
  (void)optional_actions;

  return ioctl(fd, TCSETS, (void *)term);
}

speed_t cfgetispeed(const struct termios *term) {
  return term->c_ispeed;
}

speed_t cfgetospeed(const struct termios *term) {
  return term->c_ospeed;
}

int cfsetispeed(struct termios *term, speed_t speed) {
  term->c_ispeed = speed;
  return 0;
}

int cfsetospeed(struct termios *term, speed_t speed) {
  term->c_ospeed = speed;
  return 0;
}
