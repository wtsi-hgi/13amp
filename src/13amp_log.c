/* GPLv3 or later
 * Copyright (c) 2015 Genome Research Limited */

#include "config.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "xalloc.h"

#include "13amp.h"
#include "13amp_log.h"

/**
  @brief  Debug messages to stderr
  @param  ctx     Global context
  @param  format  Format string
  @param  ...     Variadic data
*/
int cramp_log(cramp_fuse_t* ctx, const char* format, ...) {
  int res = 0;

  /* Only output if we have to */
  if (ctx->conf->debug_level & DEBUG_ME) {
    /* Construct output format string */
    char* logfmt = XNMALLOC(strlen(format) + 9, char);
    (void)sprintf(logfmt, "13 Amp: %s\n", format);

    /* Extract the data to output */
    va_list data;
    va_start(data, format);
    res = vfprintf(stderr, logfmt, data);
    va_end(data);

    free(logfmt);
  }
  
  return res;
}
