/* See LICENSE file for license and copyright information */

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <sched.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>
#include <getopt.h>

#include "libtlb.h"
#include "cacheutils.h"
#include "statistics.h"

#define COLOR_RED     "\x1b[31m"
#define COLOR_GREEN   "\x1b[32m"
#define COLOR_YELLOW  "\x1b[33m"
#define COLOR_BLUE    "\x1b[34m"
#define COLOR_MAGENTA "\x1b[35m"
#define COLOR_CYAN    "\x1b[36m"
#define COLOR_RESET   "\x1b[0m"
#define COLOR_WHITE   "\x1b[0m"

#if RECORD_POWER == 1
#include "libpowertrace.h"
libpowertrace_session_t session;
#endif

#define TRIES 1000

#if WITH_TLB_EVICT == 1
#define AVG 1
#else
#if RECORD_POWER == 1
#define AVG 100000
#else
#define AVG 1000
#endif
#endif

#define CORE1 3

#define _STR(x) #x
#define STR(x) _STR(x)

inline __attribute__((always_inline)) void prefetch(size_t p) {
  asm volatile ("prefetcht0 (%0)" : : "r" (p));
}

static float results[TRIES];

size_t measure(size_t offset, size_t* min_p, size_t* max_p) {
  uint64_t begin = 0, end = 0;

  for (size_t i = 0; i < TRIES; i++) {
    /* Clear TLB */
#if WITH_TLB_EVICT == 1
    tlb_flush();
#endif

    /* Begin measurement */
#if RECORD_POWER == 1
    begin = libpowertrace_session_get_value(&session);
#else
    begin = rdtsc();
#endif

    for(size_t j = 0; j < AVG; j++) {
      prefetch(offset);
    }

#if RECORD_POWER == 1
    end = libpowertrace_session_get_value(&session);
#else
    end = rdtsc();
#endif

    results[i] = (float) (end - begin);
  }

  float average = 0, std_deviation = 0;
  float min = 0, max = 0;
  compute_statistics(results, TRIES, &average, NULL, &std_deviation, NULL, &min, &max);

  if (min_p) *min_p = min;
  if (max_p) *max_p = max;

  return average;
}

static void
print_help(char* argv[]) {
  fprintf(stdout, "Usage: %s [OPTIONS]\n", argv[0]);
  fprintf(stdout, "\t-c, -core <value>\t Bind to cpu (default: " STR(CORE1) ")\n");
  fprintf(stdout, "\t-h, -help\t\t Help page\n");
}

int main(int argc, char* argv[]) {
  /* Initialize */
#if WITH_TLB_EVICT == 1
  tlb_init();
#endif

  /* Parse arguments */
  size_t cpu = CORE1;

  static const char* short_options = "c:h";
  static struct option long_options[] = {
    {"cpu",             required_argument, NULL, 'c'},
    {"help",            no_argument,       NULL, 'h'},
    { NULL,             0, NULL, 0}
  };

  size_t number_of_cpus = sysconf(_SC_NPROCESSORS_ONLN);

  int c;
  while ((c = getopt_long(argc, argv, short_options, long_options, NULL)) != -1) {
    switch (c) {
      case 'c':
        cpu = atoi(optarg);
        if (cpu >= number_of_cpus) {
          fprintf(stderr, "Error: CPU %zu is not available.\n", cpu);
          return -1;
        }
        break;
      case 'h':
        print_help(argv);
        return 0;
      case ':':
        fprintf(stderr, "Error: option `-%c' requires an argument\n", optopt);
        break;
      case '?':
      default:
        fprintf(stderr, "Error: Invalid option '-%c'\n", optopt);
        return -1;
    }
  }

#if RECORD_POWER == 1
  if (argc < 2) {
    fprintf(stderr, "Usage: %s <file> [<ground truth>]\n", argv[0]);
#else
  if (argc < 1) {
    fprintf(stderr, "Usage: %s [<ground truth>]\n", argv[0]);
#endif
    return -1;
  }

  /* Initialize libpowertrace */
#if RECORD_POWER == 1
  if (libpowertrace_session_init(&session, argv[optind], POWERTRACE_MODE_DIRECT) == false) {
    fprintf(stderr, "Error: Could not initialize powertrace session\n");
    return -1;
  }
#endif

#define STEPS_BEFORE 4
#define STEPS 512

#if RECORD_POWER == 1
  size_t real = (optind + 1) < argc ? strtoull(argv[optind+1], NULL, 0) : 0;
#else
  size_t real = (optind) < argc ? strtoull(argv[optind], NULL, 0) : 0;
#endif
  size_t step = 2*1024*1024;
  size_t start = 0xffffffff80000000ull - STEPS_BEFORE * step;

  FILE *f = fopen("log.csv", "w");
  fprintf(f, "Index,Address,Time,Min,Max\n");

  /* Warm-up */
  measure(start, NULL, NULL);
  measure(start, NULL, NULL);

  size_t steps_max = STEPS + STEPS_BEFORE * 2;
  for (size_t i = 0; i < steps_max; i++) {
    size_t address = start + i * step;
    size_t min = 0;
    size_t max = 0;
    size_t m = measure(address, &min, &max);
    printf("%s%3zu/%zd %p %5zd (min: %zd, max: %zd) %s\n" COLOR_RESET,
        address == real ? COLOR_GREEN : "",
        i, steps_max, (void*) address, m, min, max,
        address == real ? "*" : ""
        );
    fflush(stdout);

    if (f != NULL) {
      fprintf(f, "%zu,%p,%zd,%zd,%zd\n", i, (void*) address, m, min, max);
    }
  }

  /* Clean-up */
#if RECORD_POWER == 1
  libpowertrace_session_clear(&session);
#endif

  if (f != NULL) {
    fclose(f);
  }

  return 0;
}
