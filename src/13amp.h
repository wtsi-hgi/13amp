/* GPLv3 or later
 * Copyright (c) 2015 Genome Research Limited */

#ifndef CRAMP_H
#define CRAMP_H

#define FUSE_USE_VERSION 26

/* offsetof */
#include <stddef.h>
#include <fuse.h>

#define LOG_DEBUG 1
#define LOG_DEBUG1 2

typedef struct cramp_fuse_conf {
  char* source;
  char* ref;
  int   debug_level;
} cramp_fuse_conf_t;

typedef struct cramp_fuse {
  cramp_fuse_conf_t* conf;
} cramp_fuse_t;

#define CRAMP_FUSE_OPT(t, p, v) { t, offsetof(cramp_fuse_conf_t, p), v }

enum {
  CRAMP_FUSE_CONF_KEY_HELP,
  CRAMP_FUSE_CONF_KEY_VERSION,
  CRAMP_FUSE_CONF_KEY_DEBUG_ALL,
  CRAMP_FUSE_CONF_KEY_FOREGROUND,
  CRAMP_FUSE_CONF_KEY_SINGLETHREAD,
  CRAMP_FUSE_CONF_KEY_DEBUG_ME,
  CRAMP_FUSE_CONF_KEY_TRACE_ME,
};

#define CRAMP_DEFAULT_SOURCE "."

int  main(int, char**);
void usage(char*);
//int  cramp_options(void*, const char*, int, struct fuse_args*);
int cramp_options(void* data, const char* arg, int key, struct fuse_args* outargs);

#endif
