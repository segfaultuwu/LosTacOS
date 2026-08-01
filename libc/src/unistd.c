#include <fcntl.h>
#include <stddef.h>

#include <sys/syscall.h>
#include <unistd.h>

int open(const char *path, int flags, ...) {
  va_list args;

  va_start(args, flags);

  /*
   * Linux:
   * open(path, flags, mode)
   *
   * mode jest potrzebny tylko przy O_CREAT.
   */

  long mode = 0;

  if (flags & O_CREAT)
    mode = va_arg(args, int);

  va_end(args);

  return (int)syscall(SYS_OPEN, (long)path, flags, mode);
}

char *getcwd(char *buf, size_t size) {
  if (!buf || size == 0)
    return NULL;

  long ret = syscall(SYS_GETCWD, (long)buf, size, 0);

  if (ret < 0)
    return NULL;

  return buf;
}

int chdir(const char *path) {
  if (!path)
    return -1;

  return syscall(SYS_CHDIR, (long)path, 0, 0);
}

int close(int fd) {
  return (int)syscall(SYS_CLOSE, fd);
}

ssize_t read(int fd, void *buf, size_t len) {
  return syscall(SYS_READ, fd, (long)buf, len);
}

ssize_t write(int fd, const void *buf, size_t len) {
  return syscall(SYS_WRITE, fd, (long)buf, len);
}

int execve(const char *path, char *const argv[], char *const envp[]) {
  return syscall(SYS_EXECVE, (long)path, (long)argv, (long)envp);
}

int pipe(int pipefd[2]) {
  return syscall(SYS_PIPE, pipefd);
}

int dup2(int oldfd, int newfd) {
  return syscall(SYS_DUP2, oldfd, newfd);
}

pid_t getpid(void) {
  return (pid_t)syscall(SYS_GETPID, 0, 0, 0);
}

int fork(void) {
  return (int)syscall(SYS_FORK, 0, 0, 0);
}

int nanosleep(const struct timespec *req, struct timespec *rem) {
  return (int)syscall(SYS_NANOSLEEP, (long)req, (long)rem, 0);
}

unsigned int sleep(unsigned int seconds) {
  syscall(SYS_SLEEP, (long)seconds * 1000, 0, 0);
  return 0;
}

pid_t wait4(pid_t pid, int *status, int options, void *rusage) {
  return (pid_t)syscall(SYS_WAIT4, pid, (long)status, options, (long)rusage);
}

off_t lseek(int fd, off_t offset, int whence) {
  return syscall(SYS_LSEEK, fd, offset, whence);
}

int sys_readdir(int fd, struct dirent *ent) {
  return (int)syscall(SYS_READDIR, fd, (long)ent, 0);
}

int unlink(const char *path) {
  if (!path)
    return -1;
  return (int)syscall(SYS_UNLINK, (long)path, 0, 0);
}

int remove(const char *path) {
  return unlink(path);
}

int rmdir(const char *path) {
  if (!path)
    return -1;
  return (int)syscall(SYS_RMDIR, (long)path, 0, 0);
}
