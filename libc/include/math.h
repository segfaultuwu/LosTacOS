#pragma once

#define HUGE_VAL __builtin_huge_val()

#define INFINITY __builtin_inf()
#define NAN __builtin_nan("")

#define M_E 2.71828182845904523536
#define M_PI 3.14159265358979323846
#define M_PI_2 1.57079632679489661923

#define DBL_MAX_PRECISION 18

double fabs(double x);
float fabsf(float x);

double floor(double x);
double ceil(double x);

double sqrt(double x);

double sin(double x);
double cos(double x);
double tan(double x);

double log(double x);
double exp(double x);
