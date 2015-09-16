/* GPLv3 or later
 * Copyright (c) 2015 Genome Research Limited */

#ifndef _CRAMP_CACHE_H
#define _CRAMP_CACHE_H

#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

#include <htslib/khash.h>

/**
  @brief   CRAM file statistics
  @var     mtime  Last modified time
  @var     size   File size (0 => FIFO)
  @var     TODO   (For future use)
*/
typedef struct cramp_stat {
  time_t mtime;
  off_t  size;
  void*  TODO;
} cramp_stat_t;

/* Initialise hash table type */
KHASH_MAP_INIT_STR(stat_hash, cramp_stat_t*)
typedef khash_t(stat_hash) cramp_cache_t;

extern int           cramp_cache_put(cramp_cache_t*, const char*, cramp_stat_t*);
extern cramp_stat_t* cramp_cache_get(cramp_cache_t*, const char*);
extern void          cramp_cache_destroy(cramp_cache_t*);

extern struct stat*  cramp_cache_stat(struct stat*, cramp_stat_t*);

extern const char*   cramp_cache_file(const char*);
extern ssize_t       cramp_cache_read(const char*, cramp_cache_t*);
extern ssize_t       cramp_cache_write(const char*, cramp_cache_t*);

#endif
