#include <stdio.h>
#include <string.h>
#include <math.h>
#include "cacheutils.h"
#include <time.h>

#define REPEAT 1000000

inline __attribute__((always_inline)) void prefetch(void* p) {
  /* asm volatile ("prefetchnta (%0)" : : "r" (p)); */
  asm volatile ("prefetcht0 (%0)" : : "r" (p));
}

char __attribute__((aligned(4096))) dummy[4096];

uint64_t rdtscnf1() {
    struct timespec t1;
    clock_gettime(CLOCK_MONOTONIC, &t1);
    return t1.tv_sec * 1000 * 1000 * 1000ULL + t1.tv_nsec;
}

float results[REPEAT];

#define REP8(i) i i i i i i i i

#define SEP printf("----------------------------------------------------\n");

void compute_statistics(float* values, size_t n, float* average, float* variance, float* std_deviation, size_t threshold, size_t* new_n)
{
  float sum = 0.0;
  size_t _n = 0;

  for (size_t i = 0; i < n; i++) {
    if (values[i] < (float) threshold){
      sum += values[i];
      _n++;
    }
  }

  if (new_n != NULL) {
    *new_n = _n;
  }

  float _average = sum / (float) _n;

  float sum1 = 0.0;
  for (size_t i = 0; i < n; i++) {
    if (values[i] < (float) threshold){
      sum1 += pow(values[i] - _average, 2);
    }
  }

  float _variance = sum1 / (float) _n;

  if (average != NULL) {
    *average = _average;
  }

  if (variance != NULL) {
    *variance = _variance;
  }

  if (std_deviation != NULL) {
    *std_deviation = sqrt(_variance);
  }
}

volatile size_t start, end;

int main() {
    memset(dummy, 1, sizeof(dummy));
    printf("\n");

#define MEASURE_START() \
    asm volatile(".align 4096"); \
    for (size_t i = 0; i < REPEAT; i++) { \
	flush(dummy);\
	flush(dummy);\
        asm volatile("lfence"); \
        asm volatile("mfence"); \
        start = rdtsc();

#define MEASURE_END(txt) \
        end = rdtsc(); \
        results[i] = (float) (end - start); \
    } \
    { \
      float average = 0, std_deviation = 0; size_t new_n = 0; \
      compute_statistics(results, REPEAT, &average, NULL, &std_deviation, 100000, &new_n); \
      fprintf(stderr, "%40s: %.2f (s=%4.2f, n=%zu)\n", txt, average, std_deviation, new_n); \
    }

    MEASURE_START()
        asm volatile ("CPUID\n\t" :: "a"(0), "b"(0), "c"(0), "d"(0));
    MEASURE_END("cpuid")

    MEASURE_START()
        maccess(dummy);
        asm volatile("nop");
        asm volatile ("CPUID\n\t" :: "a"(0), "b"(0), "c"(0), "d"(0));
    MEASURE_END("flush + access + cpuid")

    MEASURE_START()
        maccess(dummy);
        asm volatile("lfence");
    MEASURE_END("flush + access + lfence")

    MEASURE_START()
        maccess(dummy);
        asm volatile("mfence");
    MEASURE_END("flush + access + mfence")

    SEP

    MEASURE_START()
        prefetch(dummy);
        asm volatile("nop");
        asm volatile ("CPUID\n\t" :: "a"(0), "b"(0), "c"(0), "d"(0));
    MEASURE_END("flush + prefetch + cpuid")

    MEASURE_START()
        prefetch(dummy);
        asm volatile("lfence");
    MEASURE_END("flush + prefetch + lfence")

    MEASURE_START()
        prefetch(dummy);
        asm volatile("mfence");
    MEASURE_END("flush + prefetch + mfence")

    printf("\n");
    return 0;
}
