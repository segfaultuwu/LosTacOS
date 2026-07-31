#pragma once

#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>

typedef struct FILE {
  int fd;
  int eof;
  int error;

  char *buffer;
  size_t buffer_size;
  size_t buffer_pos;
} FILE;

/* standard streams */
extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

/* output */
int putc(int c, FILE *stream);
int puts(const char *str);

int printf(const char *fmt, ...);
int fprintf(FILE *stream, const char *fmt, ...);

int vprintf(const char *fmt, va_list args);
int vfprintf(FILE *stream, const char *fmt, va_list args);

int snprintf(char *buf, size_t size, const char *fmt, ...);
int vsnprintf(char *buf, size_t size, const char *fmt, va_list args);

/* input */
int getchar(void);

char *fgets(char *str, int size, FILE *stream);

int scanf(const char *fmt, ...);
int fscanf(FILE *stream, const char *fmt, ...);

/* files */
FILE *fopen(const char *path, const char *mode);
int fclose(FILE *stream);

int fflush(FILE *stream);

/* status */
int feof(FILE *stream);
int ferror(FILE *stream);
void clearerr(FILE *stream);

/* buffering */
void setbuf(FILE *stream, char *buf);
int setvbuf(FILE *stream, char *buf, int mode, size_t size);
