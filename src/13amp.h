/* GPLv3 or later
 * Copyright (c) 2015 Genome Research Limited */

#ifndef CRAMP_H
#define CRAMP_H

/* Needed for offsetof */
#include <stddef.h>

#define FUSE_USE_VERSION 26
#include <fuse.h>

#define CRAMP_FUSE_OPT(t, p, v) { t, offsetof(cramp_fuse_conf_t, p), v }

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
  @var    debug_level  Debugging level
  @var    one_thread   Run single threaded
*/
typedef struct cramp_fuse_conf {
  char* source;
  int   debug_level;
  int   one_thread;
} cramp_fuse_conf_t;

/**
  @brief  13 Amp global context
  @var    conf  Pointer to configuration
  ...
*/
typedef struct cramp_fuse {
  cramp_fuse_conf_t* conf;
} cramp_fuse_t;

#endif
