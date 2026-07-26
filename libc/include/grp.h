#pragma once

#include <stddef.h>

struct group {
  char *gr_name;
  char *gr_passwd;
  unsigned int gr_gid;
  char **gr_mem;
};

struct group *getgrgid(unsigned int gid);

struct group *getgrnam(const char *name);

void setgrent(void);

void endgrent(void);

void setgrent(void);
