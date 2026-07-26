#include <pwd.h>
#include <stddef.h>

struct passwd *getpwnam(const char *name) {
  (void)name;
  return NULL;
}

struct passwd *getpwuid(uid_t uid) {
  (void)uid;
  return NULL;
}

void setpwent(void) {}

void endpwent(void) {}

struct passwd *getpwent(void) {
  return NULL;
}
