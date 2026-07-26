#pragma once

#include <stddef.h>

#define REG_NOMATCH 1
#define REG_OK 0

typedef struct {
  const char *pattern;
} regex_t;

typedef struct {
  size_t rm_so;
  size_t rm_eo;
} regmatch_t;

int regcomp(regex_t *regex, const char *pattern, int flags);

int regexec(const regex_t *regex, const char *string, size_t nmatch, regmatch_t *pmatch, int flags);

void regfree(regex_t *regex);

const char *regerror(int errcode);
