#pragma once

#include <stdint.h>

struct statfs {
  long f_type;
  long f_bsize;
  long f_blocks;
  long f_bfree;
  long f_bavail;
  long f_files;
  long f_ffree;
  long f_fsid;
  long f_namelen;
  long f_frsize;
  long f_flags;
};
