/* GPLv3 or later
 * Copyright (c) 2015 Genome Research Limited */

#ifndef CRAMP_UTIL_H
#define CRAMP_UTIL_H

/* Needed for DIR */
#include <dirent.h>

/* Needed for htsFile */
#include <htslib/hts.h>

/* Needed for fuse_file_info */
#include "13amp.h"
#include <fuse.h>

/* Get FUSE context macro */
#define CTX (cramp_fuse_t*)(fuse_get_context()->private_data)

/**
  @brief   Directory structure
  @var     dp      Directory handle
  @var     entry   Pointer to directory entity
  @var     offset  Offset
*/
struct cramp_dirp {
  DIR*           dp;
  struct dirent* entry;
  off_t          offset;
};

/**
  @brief   Directory entry structure
  @var     st       Stat structure of entry
  @var     virtual  Entry is virtual (0 = False; 1 = True)
*/
struct cramp_entry_t {
  struct stat* st;
  int          virtual;
};

/**
  @brief   File descriptor type
  @var     fd_normal  File descriptor per open(2)
  @var     fd_cram    File descriptor per hts_open
*/
enum fd_type {fd_normal, fd_cram};

/**
  @brief   File structure (tagged union of file/CRAM handle)
  @var     type   Union tag
  @var     filep  Normal file handle
  @var     cramp  CRAM file handle for HTSLib
*/
struct cramp_filep {
  enum fd_type type;
  union {
    int        filep;
    htsFile*   cramp;
  };
};

/* Utility functions to support file system operations */
extern const char* path_concat(const char*, const char*);
extern const char* source_path(const char*);
extern int         has_extension(const char*, const char*);
extern const char* sub_extension(const char*, const char*);
extern int         is_cram(const char*);

extern struct cramp_dirp*  get_dirp(struct fuse_file_info*);
extern struct cramp_filep* get_filep(struct fuse_file_info*);

#endif
