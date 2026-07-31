#include <stdio.h>
#include <stdlib.h>

static FILE stdin_obj = {
    .fd = 0, .eof = 0, .error = 0, .buffer = NULL, .buffer_size = 0, .buffer_pos = 0};

static FILE stdout_obj = {
    .fd = 1, .eof = 0, .error = 0, .buffer = NULL, .buffer_size = 0, .buffer_pos = 0};

static FILE stderr_obj = {
    .fd = 2, .eof = 0, .error = 0, .buffer = NULL, .buffer_size = 0, .buffer_pos = 0};

FILE *stdin = &stdin_obj;
FILE *stdout = &stdout_obj;
FILE *stderr = &stderr_obj;

int feof(FILE *stream) {
  if (!stream)
    return 0;

  return stream->eof;
}

int ferror(FILE *stream) {
  if (!stream)
    return 0;

  return stream->error;
}

void clearerr(FILE *stream) {
  if (!stream)
    return;

  stream->eof = 0;
  stream->error = 0;
}

void setbuf(FILE *stream, char *buf) {
  if (!stream)
    return;

  stream->buffer = buf;

  if (buf)
    stream->buffer_size = 4096;
  else
    stream->buffer_size = 0;
}

int setvbuf(FILE *stream, char *buf, int mode, size_t size) {
  (void)mode;

  if (!stream)
    return -1;

  stream->buffer = buf;
  stream->buffer_size = size;
  stream->buffer_pos = 0;

  return 0;
}
