#include <stdlib.h>

#define MAX_ENV 64

struct env_entry {
  const char *name;
  const char *value;
};

static struct env_entry env[MAX_ENV];

static int env_count = 0;

static int strcmp_(const char *a, const char *b) {
  while (*a && *b) {

    if (*a != *b)
      return 1;

    a++;
    b++;
  }

  return *a != *b;
}

char *getenv(const char *name) {
  for (int i = 0; i < env_count; i++) {

    if (!strcmp_(env[i].name, name))
      return (char *)env[i].value;
  }

  return 0;
}

int setenv(const char *name, const char *value, int overwrite) {
  for (int i = 0; i < env_count; i++) {

    if (!strcmp_(env[i].name, name)) {

      if (!overwrite)
        return 0;

      env[i].value = value;

      return 0;
    }
  }

  if (env_count >= MAX_ENV)
    return -1;

  env[env_count].name = name;
  env[env_count].value = value;

  env_count++;

  return 0;
}

int unsetenv(const char *name) {
  for (int i = 0; i < env_count; i++) {

    if (!strcmp_(env[i].name, name)) {

      for (int j = i; j < env_count - 1; j++)
        env[j] = env[j + 1];

      env_count--;

      return 0;
    }
  }

  return 0;
}
