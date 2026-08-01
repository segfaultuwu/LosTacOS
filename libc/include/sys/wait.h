#ifndef _SYS_WAIT_H
#define _SYS_WAIT_H

#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WNOHANG 1
#define WUNTRACED 2

#define WTERMSIG(status) ((status) & 0x7f)
#define WIFEXITED(status) (WTERMSIG(status) == 0)
#define WEXITSTATUS(status) (((status) & 0xff00) >> 8)
#define WIFSIGNALED(status) (((signed char)(((status) & 0x7f) + 1) >> 1) > 0)

pid_t wait4(pid_t pid, int *status, int options, void *rusage);

pid_t waitpid(pid_t pid, int *status, int options);
pid_t wait(int *status);

#ifdef __cplusplus
}
#endif

#endif // _SYS_WAIT_H
