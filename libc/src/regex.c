#include "regex.h"

static int match_here(const char *re, const char *text);

static int match_star(int c, const char *re, const char *text);

static int match_plus(int c, const char *re, const char *text);

static int match_question(int c, const char *re, const char *text);

static int match_class(const char *cls, char c, int *len);

int regcomp(regex_t *regex, const char *pattern, int flags) {
  (void)flags;

  if (!regex || !pattern)
    return REG_NOMATCH;

  regex->pattern = pattern;

  return REG_OK;
}

int regexec(const regex_t *regex, const char *string, size_t nmatch, regmatch_t *pmatch,
            int flags) {
  (void)nmatch;
  (void)pmatch;
  (void)flags;

  const char *re = regex->pattern;

  if (*re == '^')
    return match_here(re + 1, string) ? REG_OK : REG_NOMATCH;

  do {

    if (match_here(re, string))
      return REG_OK;

  } while (*string++);

  return REG_NOMATCH;
}

void regfree(regex_t *regex) {
  (void)regex;
}

const char *regerror(int errcode) {
  switch (errcode) {

  case REG_OK:
    return "success";

  case REG_NOMATCH:
    return "no match";

  default:
    return "regex error";
  }
}

static int match_here(const char *re, const char *text) {

  if (*re == 0)
    return 1;

  if (re[1] == '*')
    return match_star(re[0], re + 2, text);

  if (re[1] == '+')
    return match_plus(re[0], re + 2, text);

  if (re[1] == '?')
    return match_question(re[0], re + 2, text);

  if (*re == '$' && re[1] == 0)
    return *text == 0;

  if (*text && (*re == '.' || *re == *text))
    return match_here(re + 1, text + 1);

  if (*text && *re == '[') {

    int len;

    if (match_class(re, *text, &len))
      return match_here(re + len, text + 1);
  }

  return 0;
}

static int match_star(int c, const char *re, const char *text) {

  do {

    if (match_here(re, text))
      return 1;

  } while (*text && (c == '.' || *text++ == c));

  return 0;
}

static int match_plus(int c, const char *re, const char *text) {

  if (!*text)
    return 0;

  if (!(c == '.' || *text == c))
    return 0;

  return match_star(c, re, text + 1);
}

static int match_question(int c, const char *re, const char *text) {

  if (match_here(re, text))
    return 1;

  if (*text && (c == '.' || *text == c))
    return match_here(re, text + 1);

  return 0;
}

static int match_class(const char *cls, char c, int *len) {
  int negate = 0;
  int found = 0;

  cls++; // '['

  if (*cls == '^') {
    negate = 1;
    cls++;
  }

  while (*cls && *cls != ']') {

    if (cls[1] == '-' && cls[2] != ']') {

      if (c >= cls[0] && c <= cls[2])
        found = 1;

      cls += 3;
      continue;
    }

    if (*cls == c)
      found = 1;

    cls++;
  }

  while (*cls && *cls != ']')
    cls++;

  if (*cls == ']')
    cls++;

  *len = cls - (cls - 1);

  return negate ? !found : found;
}
