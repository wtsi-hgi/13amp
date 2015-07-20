/* GPLv3 or later
 * Copyright (c) 2015 Genome Research Limited */

/* FUSE operations derived from fusexmp.c
 * Copyright (c) 2001-2007 Miklos Szeredi */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>

#include "13amp.h"

#include <fuse.h>

/**
  @brief   Initialise filesystem
  @param   conn  FUSE connection info
  @return  ...
*/
void* cramp_init(struct fuse_conn_info* conn) {
  /* TODO */
}

/**
  @brief   Get file attributes
  @param   path   File path
  @param   stbuf  stat buffer
  @return  Exit status (0 = OK; -errno = not so much)
*/
int cramp_getattr(const char* path, struct stat* stbuf) {
  cramp_fuse_t *cf = (cramp_fuse_t*)(fuse_get_context()->private_data);
  char* realpath;
  if (asprintf(&realpath, "%s/%s", cf->conf->source, path) < 1) {
    (void)fprintf(stderr, "mem fail");
    abort();
  }

  int res = lstat(realpath, stbuf);

  if (res == -1) {
    return -errno;
  }

  free(realpath);
  return 0;
}

/**
  @brief   Open file
  @param   path  File path
  @param   fi    FUSE file info
  @return  Exit status (0 = OK; -errno = not so much)
*/
int cramp_open(const char* path, struct fuse_file_info* fi) {
  cramp_fuse_t *cf = (cramp_fuse_t*)(fuse_get_context()->private_data);
  char* realpath;
  if (asprintf(&realpath, "%s/%s", cf->conf->source, path) < 1) {
    (void)fprintf(stderr, "mem fail");
    abort();
  }

  (void)fprintf(stderr, "flags: 0x%x\n", fi->flags);

  int res = open(realpath, fi->flags);

  if (res == -1) {
    return -errno;
  } else {
    fi->fh = res;
  }

  free(realpath);
  return 0;
}

/**
  @brief   Read data from an open file
  @param   path    File path
  @param   buf     Data buffer
  @param   size    Data size (bytes)
  @param   offset  Data offset (bytes)
  @param   fi      FUSE file info
  @return  Exit status (0 = OK; -errno = not so much)
*/
int cramp_read(const char* path, char* buf, size_t size, off_t offset, struct fuse_file_info* fi) {
  /*
  cramp_fuse_t *cf = (cramp_fuse_t*)(fuse_get_context()->private_data);
  char* realpath;
  if (asprintf(&realpath, "%s/%s", cf->conf->source, path) < 1) {
    (void)fprintf(stderr, "mem fail");
    abort();
  }

  int fd, res;

  (void)fi;
  
  fd = open(realpath, O_RDONLY);
  if (fd == -1) {
    return -errno;
  }
  */

  int res;
  
  /* TODO uint */
  if (fi->fh < 0) {
    return -EBADF;
  }

  res = pread(fi->fh, buf, size, offset);
  if (res == -1) {
    return -errno;
  }

  //free(realpath);
  return res;
}

/**
  @brief   Read directory
  @param   path    File path
  @param   buf     Data buffer
  @param   filler  Function to add a readdir entry
  @param   offset  Offset of next entry
  @param   fi      FUSE file info
  @return  Exit status (0 = OK; -errno = not so much)
*/
int cramp_readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fi) {
  cramp_fuse_t *cf = (cramp_fuse_t*)(fuse_get_context()->private_data);

  DIR* dp;
  struct dirent* de;

  (void)offset;
  (void)fi;

  char* realpath;
  if (asprintf(&realpath, "%s/%s", cf->conf->source, path) < 1) {
    (void)fprintf(stderr, "mem fail");
    abort();
  }

  dp = opendir(realpath);
  if (dp == NULL) {
    return -errno;
  }

  while ((de = readdir(dp)) != NULL) {
    struct stat st;
    memset(&st, 0, sizeof(st));
    
    st.st_ino = de->d_ino;
    st.st_mode = de->d_type << 12;

    if (filler(buf, de->d_name, &st, 0)) {
      break;
    }
  }

  closedir(dp);
  free(realpath);
  return 0;
}

/**
  @brief   Release an open file
  @param   path  File path
  @param   fi    FUSE file info
  @return  Exit status (0 = OK; -errno = not so much)
*/
int cramp_release(const char* path, struct fuse_file_info* fi) {
  (void)path;
  // (void)fi;

  /* TODO uint */
  if (fi->fh < 0) {
    return -EBADF;
  }

  return close(fi->fh);

//  return 0;
}

/**
  @brief   Clean up filesystem on exit
  @param   data  ...
*/
void cramp_destroy(void* data) {
  /* TODO */
}
