#ifndef PERFORMANCE_COUNTER_H
#define PERFORMANCE_COUNTER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <errno.h>
#include <linux/hw_breakpoint.h>
#include <linux/perf_event.h>
#include <sys/syscall.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#define PERF_RAW_EVENT(sel, umask) \
  ((sel) | (((umask) << 8)))

#define PERF_CACHE_TYPE(id, op_id, op_result_id) \
  ((id) | ((op_id) << 8) | ((op_result_id) << 16))

typedef struct performance_counter_read_format_s {
  uint64_t nr;
  struct {
    uint64_t value;
    uint64_t id;
  } values[];
} performance_counter_read_format_t;

int performance_counter_open(size_t pid, size_t config) {
    struct perf_event_attr pe_attr;
    memset(&pe_attr, 0, sizeof(struct perf_event_attr));

    pe_attr.type = PERF_TYPE_RAW;
    pe_attr.size = sizeof(pe_attr);
    pe_attr.config = config;
    pe_attr.exclude_kernel = 1;
    pe_attr.exclude_hv = 1;
    pe_attr.exclude_callchain_kernel = 1;

    int fd = syscall(__NR_perf_event_open, &pe_attr, pid, -1, -1, 0);
    if (fd == -1) {
        fprintf(stderr, "[*] perf_event_open failed: %s\n", strerror(errno));
    }
    assert(fd >= 0);

    return fd;
}

void performance_counter_reset(int fd) {
  int rc = ioctl(fd, PERF_EVENT_IOC_RESET, 0);
  assert(rc == 0);
}

void performance_counter_enable(int fd) {
  int rc = ioctl(fd, PERF_EVENT_IOC_ENABLE, 0);
  assert(rc == 0);
}

void performance_counter_disable(int fd) {
  int rc = ioctl(fd, PERF_EVENT_IOC_DISABLE, 0);
  assert(rc == 0);
}

size_t performance_counter_read(int fd) {
  size_t count;
  int got = read(fd, &count, sizeof(count));
  assert(got == sizeof(count));

  return count;
}

#define PERFORMANCE_COUNTER_MAX_COUNTERS 16

typedef struct performance_counter_group_counter_s {
  int fd;
  uint64_t id;
  const char* name;
} performance_counter_group_counter_t;

typedef struct performance_counter_group_s {
  size_t n;
  int fd;
  pid_t pid;
  performance_counter_group_counter_t counter[PERFORMANCE_COUNTER_MAX_COUNTERS];
} performance_counter_group_t;

performance_counter_group_t performance_counter_group_init(size_t pid) {
  performance_counter_group_t group;
  group.n = 0;
  group.fd = -1;
  group.pid = pid;

  return group;
}

bool performance_counter_group_add(performance_counter_group_t* group, size_t config, const char* name) {
    if (group->n - 1 == PERFORMANCE_COUNTER_MAX_COUNTERS) {
      return false;
    }

    struct perf_event_attr pe_attr;
    memset(&pe_attr, 0, sizeof(struct perf_event_attr));

    pe_attr.type = PERF_TYPE_RAW;
    pe_attr.size = sizeof(pe_attr);
    pe_attr.config = config;
    pe_attr.exclude_kernel = 1;
    pe_attr.exclude_hv = 1;
    pe_attr.exclude_callchain_kernel = 1;
    pe_attr.read_format = PERF_FORMAT_GROUP | PERF_FORMAT_ID;

    int group_counter_fd = (group->n > 0) ? group->counter[0].fd : -1;
    int fd = syscall(__NR_perf_event_open, &pe_attr, group->pid, -1, group_counter_fd, 0);
    if (fd == -1) {
        fprintf(stderr, "[*] perf_event_open failed: %s\n", strerror(errno));
        return false;
    }

    group->counter[group->n].fd = fd;
    group->counter[group->n].name = name;

    if (group->n == 0) {
      group->fd = fd;
    }

    ioctl(fd, PERF_EVENT_IOC_ID, &(group->counter[group->n].id));

    /* Increase number of events in groups */
    group->n++;

    return true;
}

void performance_counter_group_reset(performance_counter_group_t* group) {
  int rc = ioctl(group->fd, PERF_EVENT_IOC_RESET, PERF_IOC_FLAG_GROUP);
  assert(rc == 0);
}

void performance_counter_group_enable(performance_counter_group_t* group) {
  int rc = ioctl(group->fd, PERF_EVENT_IOC_ENABLE, PERF_IOC_FLAG_GROUP);
  assert(rc == 0);
}

void performance_counter_group_disable(performance_counter_group_t* group) {
  int rc = ioctl(group->fd, PERF_EVENT_IOC_DISABLE, PERF_IOC_FLAG_GROUP);
  assert(rc == 0);
}

typedef struct performance_counter_group_values_s {
 uint64_t values[PERFORMANCE_COUNTER_MAX_COUNTERS];
} performance_counter_group_values_t;


bool performance_counter_group_read(performance_counter_group_t* group, performance_counter_group_values_t* values) {
  /* Read result buffer */
  char buffer[4096] = {0};
  int read_bytes = read(group->fd, &buffer, sizeof(buffer));
  int should_read_bytes = group->n * sizeof(uint64_t) * 2 + sizeof(uint64_t);
  assert(read_bytes == should_read_bytes);

  /* Parse results */
  performance_counter_read_format_t* rf = (performance_counter_read_format_t*) buffer;

  for (int i = 0; i < rf->nr; i++) { // TODO: improve better search
    for (int g = 0; g < group->n; g++) {
      if (group->counter[g].id == rf->values[i].id) {
        // memcpy((uint64_t*) values + g, &rf->values[i].value, sizeof(uint64_t));
        values->values[i] = rf->values[i].value;
        break;
      }
    }
  }

  return true;
}

performance_counter_group_values_t performance_counter_group_values_diff(performance_counter_group_t* group,
    performance_counter_group_values_t begin,
    performance_counter_group_values_t end) {
  performance_counter_group_values_t diff;
  for (size_t i = 0; i < group->n; i++) {
    diff.values[i] = end.values[i] - begin.values[i];
  }

  return diff;
}

#ifdef __cplusplus
}
#endif

#endif
