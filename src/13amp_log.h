/* GPLv3 or later
 * Copyright (c) 2015 Genome Research Limited */

#ifndef CRAMP_LOG_H
#define CRAMP_LOG_H

#include "13amp.h"

/* Debug level flags (additive) */
#define DEBUG_FUSE (1 << 0)
#define DEBUG_ME   (1 << 1)
#define DEBUG_ALL  (DEBUG_FUSE | DEBUG_ME)

/* Logging function */
extern int cramp_log(cramp_fuse_t*, const char*, ...);

#endif
