#pragma once

#include <stdint.h>

typedef unsigned int tcflag_t;
typedef unsigned char cc_t;
typedef unsigned int speed_t;

#define NCCS 32

struct termios {
  tcflag_t c_iflag;
  tcflag_t c_oflag;
  tcflag_t c_cflag;
  tcflag_t c_lflag;

  cc_t c_line;
  cc_t c_cc[NCCS];

  speed_t c_ispeed;
  speed_t c_ospeed;
};

/* ioctl requests */
#define TCGETS 0x5401
#define TCSETS 0x5402
#define TCSETSW 0x5403
#define TCSETSF 0x5404

/* tcsetattr actions */
#define TCSANOW 0
#define TCSADRAIN 1
#define TCSAFLUSH 2

/* input flags */
#define IGNBRK 0000001
#define BRKINT 0000002
#define IGNPAR 0000004
#define PARMRK 0000010
#define INPCK 0000020
#define ISTRIP 0000040
#define INLCR 0000100
#define IGNCR 0000200
#define ICRNL 0000400
#define IXON 0002000

/* output flags */
#define OPOST 0000001
#define ONLCR 0000004

/* local flags */
#define ISIG 0000001
#define ICANON 0000002
#define ECHO 0000010
#define ECHOE 0000020
#define ECHOK 0000040
#define ECHONL 0000100
#define IEXTEN 0100000

/* control flags */
#define CSIZE 0000060
#define CS5 0000000
#define CS6 0000020
#define CS7 0000040
#define CS8 0000060
#define PARENB 0000400

/* control chars */
#define VINTR 0
#define VQUIT 1
#define VERASE 2
#define VKILL 3
#define VEOF 4
#define VTIME 5
#define VMIN 6

#ifdef __cplusplus
extern "C" {
#endif

int tcgetattr(int fd, struct termios *term);
int tcsetattr(int fd, int optional_actions, const struct termios *term);

void cfmakeraw(struct termios *term);

speed_t cfgetispeed(const struct termios *term);
speed_t cfgetospeed(const struct termios *term);

int cfsetispeed(struct termios *term, speed_t speed);
int cfsetospeed(struct termios *term, speed_t speed);

#ifdef __cplusplus
}
#endif
