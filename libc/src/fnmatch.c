#include <fnmatch.h>

static int match(const char *p, const char *s, int flags);

static int match_class(const char **p, char c) {
  int ok = 0;
  int invert = 0;

  (*p)++; // skip [

  if (**p == '^' || **p == '!') {
    invert = 1;
    (*p)++;
  }

  while (**p && **p != ']') {

    if ((*p)[1] == '-' && (*p)[2] != ']') {

      if (c >= (*p)[0] && c <= (*p)[2])
        ok = 1;

      *p += 3;
      continue;
    }

    if (**p == c)
      ok = 1;

    (*p)++;
  }

  while (**p && **p != ']')
    (*p)++;

  if (**p == ']')
    (*p)++;

  return invert ? !ok : ok;
}

static int match(const char *p, const char *s, int flags) {

  while (*p) {

    switch (*p) {

    case '?':
      if (!*s)
        return 0;

      if ((flags & FNM_PATHNAME) && *s == '/')
        return 0;

      p++;
      s++;
      break;

    case '*': {
      while (*p == '*')
        p++;

      if (!*p)
        return 1;

      while (*s) {

        if ((flags & FNM_PATHNAME) && *s == '/')
          break;

        if (match(p, s, flags))
          return 1;

        s++;
      }

      return match(p, s, flags);
    }

    case '[':

      if (!*s)
        return 0;

      if (!match_class(&p, *s))
        return 0;

      s++;
      break;

    case '\\':

      if (!(flags & FNM_NOESCAPE))
        p++;

      if (*p != *s)
        return 0;

      p++;
      s++;
      break;

    default:

      if (*p != *s)
        return 0;

      p++;
      s++;
      break;
    }
  }

  return *s == 0;
}

int fnmatch(const char *pattern, const char *string, int flags) {
  return match(pattern, string, flags) ? 0 : FNM_NOMATCH;
}
