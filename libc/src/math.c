#include <math.h>

double fabs(double x) {
  return x < 0 ? -x : x;
}

float fabsf(float x) {
  return x < 0 ? -x : x;
}

double floor(double x) {
  long i = (long)x;

  if (x < 0 && x != i)
    return i - 1;

  return i;
}

double ceil(double x) {
  long i = (long)x;

  if (x > 0 && x != i)
    return i + 1;

  return i;
}

double sqrt(double x) {
  if (x <= 0)
    return 0;

  double r = x;

  for (int i = 0; i < 16; i++)
    r = (r + x / r) / 2;

  return r;
}

double sin(double x) {
  return 0;
}

double cos(double x) {
  return 1;
}

double tan(double x) {
  return 0;
}

double log(double x) {
  return 0;
}

double exp(double x) {
  return 1;
}
