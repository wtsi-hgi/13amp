/* GPLv3 or later
 * Copyright (c) 2015 Genome Research Limited */

#ifndef CRAMP_H
#define CRAMP_H

/* Needed for offsetof */
#include <stddef.h>

#define FUSE_USE_VERSION 26
#include <fuse.h>

/* Debug level flags (additive) */
#define DEBUG_FUSE (1 << 0)
#define DEBUG_ME   (1 << 1)

/* Default source directory */
#define CRAMP_DEFAULT_SOURCE "."

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

/* Function signatures */
extern int  main(int, char**);
extern void usage(char*);
extern int  cramp_fuse_options(void*, const char*, int, struct fuse_args*);

#endif
