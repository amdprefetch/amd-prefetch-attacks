/* See LICENSE file for license and copyright information */

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <sys/mman.h>
#include <unistd.h>
#include <float.h>
#include <assemblyline.h>

#include "cacheutils.h"

#define COLOR_RED     "\x1b[31m"
#define COLOR_GREEN   "\x1b[32m"
#define COLOR_YELLOW  "\x1b[33m"
#define COLOR_BLUE    "\x1b[34m"
#define COLOR_MAGENTA "\x1b[35m"
#define COLOR_CYAN    "\x1b[36m"
#define COLOR_RESET   "\x1b[0m"
#define COLOR_WHITE   "\x1b[0m"

#define LENGTH_BEGIN (1)
#define LENGTH_END (256)
#define TRIES (10000ull)
#define CACHED (0)
#define SAME_ADDRESS (0)

#define WITH_PREFETCH_NTA (0)
#define WITH_PREFETCH (1)
#define WITH_LOAD (1)

#define STEP_SIZE (4096)
#define INPUT_SIZE (4096*16)
#define BUFFER_SIZE (4096*64)
#define OUTLIER_THRESHOLD (20000)

#define LENGTH(x) (sizeof(x)/sizeof((x)[0]))

typedef size_t (*fnct_t)(size_t);

inline __attribute__((always_inline)) void cpuid(void) {
  asm volatile ("CPUID\n\t" :: "a"(0), "b"(0), "c"(0), "d"(0));
}

#define ADD_CODE(p, s) \
{ \
  size_t __l = strlen(s); \
  memcpy(p, s, __l); \
  p += __l; \
  instruction_count++; \
}

#define ADD_CODE_NC(p, s) \
{ \
  size_t __l = strlen(s); \
  memcpy(p, s, __l); \
  p += __l; \
}

#define MEASURE_BEGIN() \
  ADD_CODE_NC(p, "mov r8, rdx\n"); \
  ADD_CODE_NC(p, "clflush [r8]\n"); \
  ADD_CODE_NC(p, "lfence\n"); \
  ADD_CODE(p, "rdtsc\n"); \
  ADD_CODE(p, "shl rdx, 0x20\n"); \
  ADD_CODE(p, "or rax, rdx\n"); \
  ADD_CODE(p, "mov r9, rax\n"); \
  ADD_CODE(p, "mov rax, 0\n"); \
  ADD_CODE(p, "movntdqa r10, [r8]\n");

#define MEASURE_END() \
  ADD_CODE(p, "rdtsc\n"); \
  ADD_CODE_NC(p, "mfence\n"); \
  ADD_CODE_NC(p, "shl rdx, 0x20\n"); \
  ADD_CODE_NC(p, "or rax, rdx\n"); \
  ADD_CODE_NC(p, "sub rax, r9\n"); \
  ADD_CODE_NC(p, "ret\n");

size_t create_code_nop(char* code_input, size_t rob_size)
{
  memset(code_input, 0, INPUT_SIZE);

  char* p = code_input;
  size_t instruction_count = 0;

  MEASURE_BEGIN();

  for (size_t i = 0; i < rob_size; i++) {
    ADD_CODE(p, "nop\n");
  }

  MEASURE_END();

  return instruction_count;
}

size_t create_code_load(char* code_input, size_t rob_size)
{
  memset(code_input, 0, INPUT_SIZE);

  char* p = code_input;
  size_t instruction_count = 0;

  MEASURE_BEGIN();

  for (size_t i = 0; i < rob_size; i++) {
#if SAME_ADDRESS == 1
    ADD_CODE(p, "mov [r8], r15\n");
#else
    char _buffer[128] = {0};
    sprintf(_buffer, "mov [r8 + 0x%x], r15\n", (unsigned int) i * 4096);
    ADD_CODE(p, _buffer);
#endif
  }

  MEASURE_END();

  return instruction_count;
}

size_t create_code_prefetch(char* code_input, size_t rob_size)
{
  memset(code_input, 0, INPUT_SIZE);

  char* p = code_input;
  size_t instruction_count = 0;

  MEASURE_BEGIN();

  for (size_t i = 0; i < rob_size; i++) {
#if SAME_ADDRESS == 1
    ADD_CODE(p, "prefetcht0 [rdi]\n");
#else
    char _buffer[128] = {0};
    sprintf(_buffer, "prefetcht0 [rdi + 0x%x]\n", (unsigned int) (i * STEP_SIZE));
    ADD_CODE(p, _buffer);
#endif
  }

  MEASURE_END();

  return instruction_count;
}

void create_code_prefetchnta(char* code_input, size_t rob_size)
{
  memset(code_input, 0, INPUT_SIZE);

  char* p = code_input;
#if SAME_ADDRESS == 1
  const char* c3 = "prefetchnta [rdi];\n";
#else
  const char* c3 = "prefetchnta [rdi + 0x%x];\n";
#endif
  size_t l = strlen(c3);
  size_t written = 0;
  for (size_t i = 0; i < rob_size; i++) {
    size_t offset = i * STEP_SIZE;
    char _buffer[128] = {0};
    sprintf(_buffer, c3, offset);
    l = strlen(_buffer);
    memcpy(p + written, _buffer, l);
    written += l;
  }
  memcpy(p + written, "ret\n", 4);
}

/* Forward declarations */
static void compute_statistics(float* values, size_t n, size_t* new_n, float* average, float* min, float* variance, float* std_deviation, float* std_error);

static float results[TRIES];

float measure_fnc(char* buffer, size_t rob_size, fnct_t fnc) {
  /* Clear results */
  memset(results, 0, TRIES * sizeof(float));

  for (size_t try = 0; try < TRIES; try++) {
#if CACHED == 0
#if SAME_ADDRESS == 1
    flush(buffer);
#else
    for (size_t i = 0; i < rob_size; i++) {
      flush(buffer + i * STEP_SIZE);
    }
#endif
#else
#if SAME_ADDRESS == 1
    maccess(buffer);
#else
    for (size_t i = 0; i < rob_size; i++) {
      maccess(buffer + i * STEP_SIZE);
    }
#endif
#endif
    asm volatile("mfence");
    asm volatile("lfence");
    cpuid();

    size_t value = 0;
    asm volatile(
        "call *%[fnc]\n"
        : "=a"(value) : [fnc]"p"(fnc), "d"(buffer) : "rbx", "rcx", "r8", "r9", "r10"
        );
    results[try] = value;
  }

  float average = 0, std_error = 0, min = 0;
  size_t new_n = 0;
  compute_statistics(results, TRIES, &new_n, &average, &min, NULL, NULL, &std_error);

  return average;
}

int main(int argc, char* argv[])
{
  size_t number_of_pages = (LENGTH_END - LENGTH_BEGIN) + 1;
  size_t buffer_size = 4096 * number_of_pages;

  FILE* f = fopen("log.csv", "w");
#if WITH_PREFETCH_NTA == 1
  fprintf(f, "Index,Load,Prefetch,PrefetchNTA,NOP\n");
#else
  fprintf(f, "Index,Load,Prefetch,NOP\n");
#endif

  char* buffer = (char*) mmap(NULL, buffer_size, PROT_READ | PROT_WRITE | PROT_NONE, MAP_SHARED | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
  if (buffer == MAP_FAILED) {
    fprintf(stderr, "Error: Could not allocate buffer\n");
    return -1;
  }
  memset(buffer, 0xAA, buffer_size);

  /* Blah */
  uint8_t *code_buffer = mmap(NULL, sizeof(uint8_t) * BUFFER_SIZE, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  if (code_buffer == MAP_FAILED) {
    fprintf(stderr, "Failed to allocate buffer\n");
    return -1;
  }

  /* Blah */
  char*code_input = mmap(NULL, INPUT_SIZE, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  if (code_input == MAP_FAILED) {
    fprintf(stderr, "Failed to allocate buffer\n");
    return -1;
  }

  /* Run measurements */
  for (size_t rob_size = LENGTH_END; rob_size >= LENGTH_BEGIN; rob_size--) {
    assemblyline_t al = asm_create_instance(code_buffer, BUFFER_SIZE);
    memset(code_input, 0, INPUT_SIZE);
    size_t offset = create_code_nop(code_input, rob_size);
    assemble_str(al, code_input);

    fnct_t fnc = (fnct_t) asm_get_code(al);

    float result_nop = measure_fnc(buffer, rob_size, fnc);
    asm_destroy_instance(al);

    /* Load */
#if WITH_LOAD == 1
    al = asm_create_instance(code_buffer, BUFFER_SIZE);
    create_code_load(code_input, rob_size);
    assemble_str(al, code_input);
    fnc = (fnct_t) asm_get_code(al);

    float result_load = measure_fnc(buffer, rob_size, fnc);
    asm_destroy_instance(al);
#else
    float result_load = 0.0;
#endif

    /* Prefetch */
#if WITH_PREFETCH == 1
    al = asm_create_instance(code_buffer, BUFFER_SIZE);
    create_code_prefetch(code_input, rob_size);
    assemble_str(al, code_input);
    fnc = (fnct_t) asm_get_code(al);

    float result_prefetch = measure_fnc(buffer, rob_size, fnc);
    asm_destroy_instance(al);
#else
    float result_prefetch = 0.0;
#endif

    /* Prefetch */
#if WITH_PREFETCH_NTA == 1
    al = asm_create_instance(code_buffer, BUFFER_SIZE);
    create_code_prefetchnta(code_input, rob_size);
    assemble_str(al, code_input);
    fnc = (fnct_t) asm_get_code(al);

    float result_prefetchnta = measure_fnc(buffer, rob_size, fnc);
    asm_destroy_instance(al);
#endif

    /* Show results */
#if WITH_PREFETCH_NTA == 1
    fprintf(stderr, "%3zu: %10.3f, %10.3f, %10.3f, %10.3f\n", offset, result_load, result_prefetch, result_prefetchnta, result_nop);
    fprintf(f, "%zu,%.3f,%.3f,%.3f,%.3f\n", offset, result_load, result_prefetch, result_prefetchnta, result_nop);
  }
#else
    fprintf(stderr, "%3zu: %10.3f, %10.3f, %10.3f\n", offset, result_load, result_prefetch, result_nop);
    fprintf(f, "%zu,%.3f,%.3f,%.3f\n", offset, result_load, result_prefetch, result_nop);
  }
#endif

  /* Cleanup */
  munmap(buffer, buffer_size);
  fclose(f);

  return 0;
}

void compute_statistics(float* values, size_t n, size_t* new_n, float* average, float* min, float* variance, float* std_deviation, float* std_error)
{
  float sum = 0.0;
  float _min = values[0];
  size_t _n = 0;

  for (size_t i = 0; i < n; i++) {
    if (values[i] < OUTLIER_THRESHOLD){
      sum += values[i];
      _n++;
    }

    if (values[i] < _min) {
      _min = values[i];
    }
  }

  if (min != NULL) {
    *min = _min;
  }

  if (new_n != NULL) {
    *new_n = _n;
  }

  float _average = sum / (float) _n;

  float sum1 = 0.0;
  for (size_t i = 0; i < n; i++) {
    if (values[i] < OUTLIER_THRESHOLD){
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

  float _std_deviation = sqrt(_variance);
  if (std_deviation != NULL) {
    *std_deviation = _std_deviation;
  }

  if (std_error != NULL) {
    *std_error = _std_deviation / sqrtf(n);
  }
}
