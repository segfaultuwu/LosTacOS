#pragma once

#include <stddef.h>
#include <stdint.h>

/*
 * Memory management
 */

extern void *malloc(size_t size);

// extern void *calloc(size_t count, size_t size);

// extern void *realloc(void *ptr, size_t size);

extern void free(void *ptr);

/*
 * Program control
 */

extern void exit(int status);

extern void abort(void);

/*
 * Conversion
 */

extern int atoi(const char *str);

extern long atol(const char *str);

extern long strtol(const char *str, char **endptr, int base);

/*
 * Random
 */

extern int rand(void);

extern void srand(unsigned int seed);

/*
 * Absolute value
 */

extern int abs(int value);

extern long labs(long value);

/*
 * Environment
 */

extern char *getenv(const char *name);

extern int setenv(const char *name, const char *value, int overwrite);

extern int unsetenv(const char *name);
