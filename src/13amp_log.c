/* GPLv3 or later
 * Copyright (c) 2015 Genome Research Limited */

#include "config.h"

#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include "xvasprintf.h"

#include "13amp.h"
#include "13amp_log.h"

#include <fuse.h>

/**
  @brief  Debug messages to stderr
  @param  format  Format string
  @param  ...     Variadic data

  NOTE: This can only be run during the FUSE session
*/
void cramp_log(const char* format, ...) {
  cramp_fuse_t* ctx = (cramp_fuse_t*)(fuse_get_context()->private_data);

  /* Only output if we have to */
  if (ctx->conf->debug_level & DEBUG_ME) {
    /* Construct output format string */
    char* logfmt = xasprintf("13 Amp: %s\n", format);
    if (logfmt == NULL) {
      (void)fprintf(stderr, "13 Amp FATAL: Memory allocation failure\n");
      exit(errno);
    }

    /* Extract the data to output */
    va_list data;
    va_start(data, format);
    (void)vfprintf(stderr, logfmt, data);
    va_end(data);

    free(logfmt);
  }
}

/**
  @brief  Fatal messages to stderr then exit
  @param  format  Format string
  @param  ...     Variadic data

  NOTE: This is context-less and can be run anywhere
*/
void cramp_log_fatal(const char* format, ...) {
  char* logfmt = xasprintf("13 Amp FATAL: %s\n", format);
  if (logfmt == NULL) {
    (void)fprintf(stderr, "13 Amp FATAL: Memory allocation failure\n");
    exit(errno);
  }

  va_list data;
  va_start(data, format);
  (void)vfprintf(stderr, logfmt, data);
  va_end(data);

  free(logfmt);
  exit(errno ? errno : EFATAL); /* I wanna use the Elvis operator ?: */
}

/**
  This is called by the xalloc functions in the event of a memory
  allocation failure. We want to keep using the xalloc functions, but
  handle failures ourselves/pass them to FUSE.
*/
void xalloc_die(void) {}
