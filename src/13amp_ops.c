/* GPLv3 or later
 * Copyright (c) 2015 Genome Research Limited */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>

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
  int res = lstat(path, stbuf);

  if (res == -1) {
    return -errno;
  }

  return 0;
}

/**
  @brief   Open file
  @param   path  File path
  @param   fi    FUSE file info
  @return  Exit status (0 = OK; -errno = not so much)
*/
int cramp_open(const char* path, struct fuse_file_info* fi) {
  int res = open(path, fi->flags);

  if (res == -1) {
    return -errno;
  }

  close(res);
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
  int fd, res;

  (void)fi;

  fd = open(path, O_RDONLY);
  if (fd == -1) {
    return -errno;
  }

  res = pread(fd, bug, size, offset);
  if (res == -1) {
    return -errno;
  }

  close(fd);
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
  DIR* dp;
  struct dirent* de;

  (void)offset;
  (void)fi;

  dp = opendir(path);
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
  return 0;
}

/**
  @brief   Release an open file
  @param   path  File path
  @param   fi    FUSE file info
  @return  Exit status (0 = OK; -errno = not so much)
*/
int cramp_release(const char* path, struct fuse_file_info* fi) {
  /* XXX Does this need to be implemented? */
  (void)path;
  (void)fi;
  return 0;
}

/**
  @brief   Clean up filesystem on exit
  @param   data  ...
*/
void cramp_destroy(void* data) {
  /* TODO */
}
