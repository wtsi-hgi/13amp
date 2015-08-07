/* GPLv3 or later
 * Copyright (c) 2015 Genome Research Limited */

#ifndef CRAMP_UTIL_H
#define CRAMP_UTIL_H

/* Needed for DIR */
#include <dirent.h>

/* Needed for htsFile */
#include <htslib/hts.h>

/* XXX How come I don't need to include fuse.h for fuse_file_info? */

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


/**** TODO START ****/

enum fpType { fpNorm, fpCRAM };

/**
  @brief   File structure
  @var     path   File path
  @var     kind   Union tag
  @var     filep  Normal file handle
  @var     cramp  CRAM file handle
*/
struct cramp_filep {
  const char* path;
  enum fpType kind;
  union {
    int       filep;
    htsFile*  cramp;
  };
};

/**** TODO END ****/

/* Utility functions to support file system operations */
extern const char* path_concat(const char*, const char*);
extern const char* source_path(const char*);
extern int         has_extension(const char*, const char*);
extern const char* sub_extension(const char*, const char*);
extern int         is_cram(const char*);

extern struct cramp_filep* get_filep(struct fuse_file_info*);
extern struct cramp_dirp*  get_dirp(struct fuse_file_info*);

#endif
