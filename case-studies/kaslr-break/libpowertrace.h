 /* See LICENSE file for license and copyright information */

#ifndef LIBPOWERTRACE_H
#define LIBPOWERTRACE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

typedef enum libpowertrace_mode_e {
  POWERTRACE_MODE_DIRECT = 0,
  POWERTRACE_MODE_DIFF,
} libpowertrace_mode_t;

typedef struct libpowertrace_session_s {
  int fd;
  const char* filename;
  libpowertrace_mode_t mode;
  uint64_t previous_value;
} libpowertrace_session_t;

bool libpowertrace_session_init(libpowertrace_session_t* session, const char* filename, libpowertrace_mode_t mode);
bool libpowertrace_session_clear(libpowertrace_session_t* session);
uint64_t libpowertrace_session_get_value(libpowertrace_session_t* session);

#ifdef __cplusplus
}
#endif

#endif

/* forward declaration */
static uint64_t file_read_line(int fd);

bool libpowertrace_session_init(libpowertrace_session_t* session, const char* filename, libpowertrace_mode_t mode)
{
  if (session == NULL || filename == NULL) {
    return false;
  }

  session->fd = open(filename, O_RDONLY);
  if (session->fd == -1) {
    return false;
  }

  session->filename = filename;
  session->mode = mode;

  session->previous_value = file_read_line(session->fd);

  return true;
}

bool libpowertrace_session_clear(libpowertrace_session_t* session)
{
  if (session == NULL) {
    return false;
  }

  close(session->fd);

  return true;
}

uint64_t libpowertrace_session_get_value(libpowertrace_session_t* session) {
  uint64_t value = file_read_line(session->fd);

  if (session->mode == POWERTRACE_MODE_DIRECT) {
    return value;
  } else if (session->mode == POWERTRACE_MODE_DIFF) {
    uint64_t previous_value = session->previous_value;
    session->previous_value = value;
    return value - previous_value;
  }

  return 0;
}

static uint64_t file_read_line(int fd)
{
  /* Read line */
  char buffer[100];

  char c;
  int i = 0;
  while (true) {
    if (read(fd, &c, 1) < 1) {
      continue;
    }
    if (c == '\n') {
      break;
    }

    buffer[i++] = c;
  }

  buffer[i] = '\0';

  /* Seek to beginning of file */
  lseek(fd, SEEK_SET, 0);

  /* Convert line */
  return strtol(buffer, NULL, 10);
}
