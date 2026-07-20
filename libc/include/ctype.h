#pragma once

static inline int isdigit(int c) {
  return c >= '0' && c <= '9';
}

static inline int isspace(int c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}
