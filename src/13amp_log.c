/* GPLv3 or later
 * Copyright (c) 2015 Genome Research Limited */

#include "config.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
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

    /* Extract the data to output */
    va_list data;
    va_start(data, format);
    (void)vfprintf(stderr, logfmt, data);
    va_end(data);

    free(logfmt);
  }
}
