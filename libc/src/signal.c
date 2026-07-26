#include <signal.h>

sighandler_t signal(int sig, sighandler_t handler) {
  (void)sig;
  return handler;
}

int kill(pid_t pid, int sig) {
  (void)pid;
  (void)sig;
  return 0;
}

int sigemptyset(sigset_t *set) {
  *set = 0;
  return 0;
}

int sigfillset(sigset_t *set) {
  *set = ~0U;
  return 0;
}

int sigaddset(sigset_t *set, int sig) {
  *set |= (1U << sig);
  return 0;
}

int sigdelset(sigset_t *set, int sig) {
  *set &= ~(1U << sig);
  return 0;
}

int sigismember(const sigset_t *set, int sig) {
  return (*set & (1U << sig)) != 0;
}
