#pragma once

#include <stddef.h>
#include <stdint.h>

/*
 * Memory management
 */

void *malloc(size_t size);

void *calloc(size_t count, size_t size);

void *realloc(void *ptr, size_t size);

void free(void *ptr);

/*
 * Program control
 */

void exit(int status);

void abort(void);

/*
 * Conversion
 */

int atoi(const char *str);

long atol(const char *str);

long strtol(const char *str, char **endptr, int base);

/*
 * Random
 */

int rand(void);

void srand(unsigned int seed);

/*
 * Absolute value
 */

int abs(int value);

long labs(long value);

/*
 * Environment
 */

char *getenv(const char *name);

int setenv(const char *name, const char *value, int overwrite);

int unsetenv(const char *name);
