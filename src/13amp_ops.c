/* GPLv3 or later
 * Copyright (c) 2015 Genome Research Limited */

/* FUSE operations derived from fusexmp.c
 * Copyright (c) 2001-2007 Miklos Szeredi */

#include "config.h"

#include <stdlib.h>

/* gnulib suggested includes */
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <locale.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>
#include "error.h"
#include "dirent-safer.h"
#include "gl_avltreehash_list.h"
#include "gl_xlist.h"
#include "hash-pjw.h"
#include "progname.h"
#include "size_max.h"
#include "version-etc.h"
#include "xalloc.h"
#include "xstrndup.h"

#include "13amp.h"
#include "13amp_log.h"

#include <fuse.h>

#include <htslib/hts.h>

/**
  @brief   Check file extension for .cram
  @param   path  File path
  @return  1 = Yep; 0 = Nope

  This is nothing more than a simple string comparison
*/
static int possibly_cram(const char* path) {
  size_t len = strlen(path);

  if (len < 5) {
    return 0;
  }

  return strcmp(path + len - 5, ".cram") == 0;
}

/**
  @brief   Check HTSLib open file is a CRAM
  @param   path  File path
  @return  1 = Yep; 0 = Nope
*/
static int actually_cram(const htsFile* fp) {
  int ret = 0;

  /* Open file and extract format */
  const htsFormat* format = hts_get_format(fp);

  if (format) {
    ret = (format->format == cram);
  }

  return ret;
}

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
  cramp_fuse_t *ctx = (cramp_fuse_t*)(fuse_get_context()->private_data);
  char* realpath;
  if (asprintf(&realpath, "%s/%s", ctx->conf->source, path) < 1) {
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
  cramp_fuse_t *ctx = (cramp_fuse_t*)(fuse_get_context()->private_data);
  char* realpath;
  if (asprintf(&realpath, "%s/%s", ctx->conf->source, path) < 1) {
    (void)fprintf(stderr, "mem fail");
    abort();
  }

  (void)fprintf(stderr, "flags: 0x%x\n", fi->flags);

  /* TEST Check CRAM file on open */
  if (possibly_cram(realpath)) {
    (void)cramp_log(ctx, "\"%s\" could be a CRAM file...", realpath);

    htsFile* fp = hts_open(realpath, "r");

    if (fp) {
      if (actually_cram(fp)) {
        (void)cramp_log(ctx, "\"%s\" actually IS a CRAM file :)", realpath);
      } else {
        (void)cramp_log(ctx, "Turns out \"%s\" isn't a CRAM file :(", realpath);
      }

      (void)hts_close(fp);
    }
  }

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
  int res;
  
  /* TODO uint */
  if (fi->fh < 0) {
    return -EBADF;
  }

  res = pread(fi->fh, buf, size, offset);
  if (res == -1) {
    return -errno;
  }

  return res;
}

/**
  @brief   Release an open file
  @param   path  File path
  @param   fi    FUSE file info
  @return  Exit status (0 = OK; -errno = not so much)
*/
int cramp_release(const char* path, struct fuse_file_info* fi) {
  (void)path;

  /* TODO uint */
  if (fi->fh == 0) {
    return -EBADF;
  }

  return close(fi->fh);
}

/**
  @brief   Open directory
  @param   path  File path
  @param   fi    FUSE file info
  @return  Exit status (0 = OK; -errno = not so much)
*/
int cramp_opendir(const char* path, struct fuse_file_info* fi) {
  /* TODO */
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
  cramp_fuse_t *ctx = (cramp_fuse_t*)(fuse_get_context()->private_data);

  DIR* dp;
  struct dirent* de;

  (void)offset;
  (void)fi;

  char* realpath;
  if (asprintf(&realpath, "%s/%s", ctx->conf->source, path) < 1) {
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
  @brief   Release an open directory
  @param   path  File path
  @param   fi    FUSE file info
  @return  Exit status (0 = OK; -errno = not so much)
*/
int cramp_releasedir(const char* path, struct fuse_file_info* fi) {
  /* TODO */
}

/**
  @brief   Clean up filesystem on exit
  @param   data  ...
*/
void cramp_destroy(void* data) {
  /* TODO */
}
