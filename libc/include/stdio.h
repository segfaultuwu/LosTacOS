#pragma once

#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>

#define EOF (-1)

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
int fputc(int c, FILE *stream);
int puts(const char *str);
int fputs(const char *str, FILE *stream);

int printf(const char *fmt, ...);
int fprintf(FILE *stream, const char *fmt, ...);

int vprintf(const char *fmt, va_list args);
int vfprintf(FILE *stream, const char *fmt, va_list args);

int snprintf(char *buf, size_t size, const char *fmt, ...);
int vsnprintf(char *buf, size_t size, const char *fmt, va_list args);

size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);

/* input */
int getchar(void);
int fgetc(FILE *stream);
int getc(FILE *stream);

char *fgets(char *str, int size, FILE *stream);
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);

int scanf(const char *fmt, ...);
int fscanf(FILE *stream, const char *fmt, ...);
int sscanf(const char *str, const char *fmt, ...);
int vsscanf(const char *str, const char *fmt, va_list args);

/* files */
FILE *fopen(const char *path, const char *mode);
int fclose(FILE *stream);
int remove(const char *pathname);

int fflush(FILE *stream);

/* status */
int feof(FILE *stream);
int ferror(FILE *stream);
void clearerr(FILE *stream);

/* buffering */
void setbuf(FILE *stream, char *buf);
int setvbuf(FILE *stream, char *buf, int mode, size_t size);
