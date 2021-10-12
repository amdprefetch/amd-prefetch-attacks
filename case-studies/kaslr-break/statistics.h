#ifndef STATISTICS_H
#define STATISTICS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <math.h>
#include <float.h>

void compute_statistics(float* values, size_t n, float* average, float* variance, float* std_deviation, float* std_error, float* min, float* max)
{
  float sum = 0.0;
  float _min = FLT_MAX;
  float _max = FLT_MIN;
  size_t _n = 0;

  for (size_t i = 0; i < n; i++) {
    float v = values[i];
    sum += v;
    _n++;

    if (v < _min) {
      _min = v;
    }

    if (v > _max) {
      _max = v;
    }
  }

  float _average = sum / (float) _n;

  float sum1 = 0.0;
  for (size_t i = 0; i < n; i++) {
    sum1 += pow(values[i] - _average, 2);
  }

  float _variance = sum1 / (float) _n;

  if (average != NULL) {
    *average = _average;
  }

  if (variance != NULL) {
    *variance = _variance;
  }

  float _std_deviation = sqrt(_variance);
  if (std_deviation != NULL) {
    *std_deviation = _std_deviation;
  }

  if (std_error != NULL) {
    *std_error = _std_deviation / sqrtf(n);
  }

  if (min != NULL) {
    *min = _min;
  }

  if (max != NULL) {
    *max = _max;
  }
}

#ifdef __cplusplus
}
#endif

#endif
