/* GPLv3 or later
 * Copyright (c) 2015 Genome Research Limited */

#ifndef CRAMP_LOG_H
#define CRAMP_LOG_H

/* Debug level flags (additive) */
#define DEBUG_FUSE (1 << 0)
#define DEBUG_ME   (1 << 1)
#define DEBUG_ALL  (DEBUG_FUSE | DEBUG_ME)

/* General fatal error exit code */
#define EFATAL -1

/* Logging function */
extern void cramp_log(const char*, ...);
extern void cramp_log_fatal(const char*, ...);

#endif
