#include <libgen.h>

char *basename(char *path) {
  static char dot[] = ".";

  if (!path || !*path)
    return dot;

  char *end = path;

  while (*end)
    end++;

  while (end > path && end[-1] == '/')
    end--;

  if (end == path)
    return dot;

  char *start = end;

  while (start > path && start[-1] != '/')
    start--;

  *end = 0;

  return start;
}

char *dirname(char *path) {
  static char dot[] = ".";

  if (!path || !*path)
    return dot;

  char *p = path;

  while (*p)
    p++;

  while (p > path && p[-1] == '/')
    p--;

  while (p > path && p[-1] != '/')
    p--;

  if (p == path)
    return dot;

  while (p > path && p[-1] == '/')
    p--;

  *p = 0;

  return path;
}
