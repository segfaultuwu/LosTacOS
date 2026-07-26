#include <grp.h>

struct group *getgrgid(unsigned int gid) {
  static struct group grp;

  grp.gr_name = "root";
  grp.gr_passwd = "";
  grp.gr_gid = gid;
  grp.gr_mem = 0;

  return &grp;
}

struct group *getgrnam(const char *name) {
  static struct group grp;

  grp.gr_name = (char *)name;
  grp.gr_passwd = "";
  grp.gr_gid = 0;
  grp.gr_mem = 0;

  return &grp;
}

void setgrent(void) {}

void endgrent(void) {}
