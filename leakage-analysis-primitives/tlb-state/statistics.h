#ifndef STATISTICS_H
#define STATISTICS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <math.h>
#include "performance-counter.h"

void compute_statistics(float* values, size_t n, size_t* new_n, float* average, float* variance, float* std_deviation, float* std_error)
{
  float sum = 0.0;
  size_t _n = 0;

  for (size_t i = 0; i < n; i++) {
    sum += values[i];
    _n++;
  }

  if (new_n != NULL) {
    *new_n = _n;
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
}

void performance_counter_group_compute_statistics(performance_counter_group_values_t* values, size_t n, size_t offset, size_t* new_n, float* average, float* variance, float* std_deviation, float* std_error)
{
  float sum = 0.0;
  size_t _n = 0;

  for (size_t i = 0; i < n; i++) {
    sum += (float) values[i].values[offset];
    _n++;
  }

  if (new_n != NULL) {
    *new_n = _n;
  }

  float _average = sum / (float) _n;

  float sum1 = 0.0;
  for (size_t i = 0; i < n; i++) {
    sum1 += pow(values[i].values[offset] - _average, 2);
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
}

#ifdef __cplusplus
}
#endif

#endif
