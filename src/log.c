/* GPLv3 or later
 * Copyright (c) 2015 Genome Research Limited */

#include "config.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "13amp.h"
#include "log.h"
#include "util.h"

/* Logging output */
#define LOG_TEXT    "13 Amp"
#define FATAL_TEXT  "FATAL"
#define DELIM_TEXT  ": "
#define CRAMP_LOG   LOG_TEXT DELIM_TEXT
#define FATAL_LOG   LOG_TEXT " " FATAL_TEXT DELIM_TEXT

/**
  @brief   Construct log format string
  @param   format  Format string
  @param   fatal   Fatal flag (0 = False; 1 = True)
  @return  malloc'd pointer to format string
*/
static const char* logfmt(const char* format, int fatal) {
  static char*  logtxt[2] = {CRAMP_LOG, FATAL_LOG};
  static size_t loglen[2] = {strlen(CRAMP_LOG), strlen(FATAL_LOG)};

  size_t fmtlen = strlen(format);
  size_t len = loglen[fatal] + fmtlen;
  const char* output = malloc(len + 2);
  if (output) {
    memcpy((void*)output, logtxt[fatal], loglen[fatal]);
    memcpy((void*)(output + loglen[fatal]), format, fmtlen);
    memset((void*)(output + len), '\n', 1);
    memset((void*)(output + len + 1), '\0', 1);
  }

  return output;
}

/**
  @brief   Debug messages to stderr
  @param   fatal   Fatal flag (0 = False; 1 = True)
  @param   format  Format string
  @param   ...     Variadic data

  Note that non-fatal logging can only be run within a FUSE session
*/
void cramp_log(int fatal, const char* format, ...) {
  int exit_code;

  if (fatal) {
    /* Save pertinent errno */
    exit_code = errno ? errno : EFATAL;
  } else {
    /* Only log non-fatal messages if we have to */
    cramp_ctx_t* ctx = CTX;
    if (!(ctx->conf->debug_level & DEBUG_ME)) {
      return;
    }
  }

  const char* fmt = logfmt(format, fatal);
  if (fmt == NULL) {
    (void)fprintf(stderr, "%sMemory allocation failure\n", FATAL_LOG);
    exit(errno);
  }

  /* Extract the data to output */
  va_list data;
  va_start(data, format);
  (void)vfprintf(stderr, fmt, data);
  va_end(data);

  free((void*)fmt);

  if (fatal) {
    exit(exit_code);
  }
}
