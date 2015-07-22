/* GPLv3 or later
 * Copyright (c) 2015 Genome Research Limited */

#include "config.h"

#include <stdio.h>
#include <stdarg.h>
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
    /* Extract the data to output */
    va_list data;
    va_start(data, format);

    (void)fprintf(stderr, "13 Amp: ");
    res = vfprintf(stderr, format, data);
    (void)fprintf(stderr, "\n");

    va_end(data);
  }
  
  return res;
}
