/* GPLv3 or later
 * Copyright (c) 2015 Genome Research Limited */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include <fuse.h>

/**
  @brief   Initialise filesystem
  @param   conn  ...
  @return  ...
*/
void* cramp_init(struct fuse_conn_info* conn) {
  /* TODO */
}

/**
  @brief   Get file attributes
  @param   path   File path
  @param   stbuf  ...
  @return  Exit status (0 = OK; -errno = not so much)
*/
int cramp_getattr(const char* path, struct stat* stbuf) {
  /* TODO */
}

/**
  @brief   Open file
  @param   path  File path
  @param   fi    ...
  @return  Exit status (0 = OK; -errno = not so much)
*/
int cramp_open(const char* path, struct fuse_file_info* fi) {
  /* TODO */
}

/**
  @brief   Read data from an open file
  @param   path    File path
  @param   buf     ...
  @param   size    ...
  @param   offset  ...
  @param   fi      ...
  @return  Exit status (0 = OK; -errno = not so much)
*/
int cramp_read(const char* path, char* buf, size_t size, off_t offset, struct fuse_file_info* fi) {
  /* TODO */
}

/**
  @brief   Read directory
  @param   path    File path
  @param   buf     ...
  @param   filler  ...
  @param   offset  ...
  @param   fi      ...
  @return  Exit status (0 = OK; -errno = not so much)
*/
int cramp_readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fi) {
  /* TODO */
}

/**
  @brief   Release an open file
  @param   path  File path
  @param   fi    ...
  @return  Exit status (0 = OK; -errno = not so much)
*/
int cramp_release(const char* path, struct fuse_file_info* fi) {
  /* TODO */
}

/**
  @brief   Clean up filesystem on exit
  @param   data  ...
*/
void cramp_destroy(void* data) {
  /* TODO */
}
