/* GPLv3 or later
 * Copyright (c) 2015 Genome Research Limited */

#ifndef _CRAMP_H
#define _CRAMP_H

/* Needed for offsetof */
#include <stddef.h>

/* Needed for off_t */
#include <sys/types.h>

#define FUSE_USE_VERSION 26
#include <fuse.h>

#define CRAMP_FUSE_OPT(t, p, v) { t, offsetof(cramp_conf_t, p), v }

/* Needed for cramp_cache_t */
#include "cache.h"

/* Option keys for FUSE */
enum {
  CRAMP_FUSE_CONF_KEY_HELP,
  CRAMP_FUSE_CONF_KEY_VERSION,
  CRAMP_FUSE_CONF_KEY_DEBUG_ALL,
  CRAMP_FUSE_CONF_KEY_DEBUG_ME,
  CRAMP_FUSE_CONF_KEY_DEBUG_FUSE,
  CRAMP_FUSE_CONF_KEY_FOREGROUND,
  CRAMP_FUSE_CONF_KEY_SINGLETHREAD
};

/**
  @brief  Runtime configuration
  @var    source       Source directory
  @var    cache        CRAM stat cache file
  @var    debug_level  Debugging level
  @var    one_thread   Run single threaded
  @var    bamsize      Default BAM file size
*/
typedef struct cramp_conf {
  const char* source;
  const char* cache;
  int         debug_level;
  int         one_thread;
  off_t       bamsize;
} cramp_conf_t;

/**
  @brief  13 Amp global context
  @var    conf   Pointer to configuration
  @var    cache  CRAM stat runtime cache
*/
typedef struct cramp_ctx {
  cramp_conf_t*  conf;
  cramp_cache_t* cache;
} cramp_ctx_t;

#endif
