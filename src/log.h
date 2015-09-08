/* GPLv3 or later
 * Copyright (c) 2015 Genome Research Limited */

#ifndef _CRAMP_LOG_H
#define _CRAMP_LOG_H

/* Debug level flags (additive) */
#define DEBUG_FUSE (1 << 0)
#define DEBUG_ME   (1 << 1)
#define DEBUG_ALL  (DEBUG_FUSE | DEBUG_ME)

/* General fatal error exit code */
#define EFATAL -1

/* Logging function macros */
#define LOG(format, ...) cramp_log(0, format, ##__VA_ARGS__)
#define WTF(format, ...) cramp_log(1, format, ##__VA_ARGS__)

/* This shouldn't need to be used directly */
extern void cramp_log(int, const char*, ...);

#endif
