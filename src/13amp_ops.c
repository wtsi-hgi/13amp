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
#include "xvasprintf.h"
#include "xstrndup.h"

#include "13amp.h"
#include "13amp_log.h"

#include <fuse.h>

#include <htslib/hts.h>

/* Get context macro */
#define CTX (cramp_fuse_t*)(fuse_get_context()->private_data)

/**
  @brief   Check file extension for .cram
  @param   path  File path
  @return  1 = Yep; 0 = Nope

  This is just a stupid-simple string comparison. It needs to be cheap,
  as it will be run regularly (e.g., on every entry in a readdir), but
  still provide a reasonable proxy to the truth.
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
static int actually_cram(htsFile* fp) {
  int ret = 0;

  /* Open file and extract format */
  const htsFormat* format = hts_get_format(fp);

  if (format) {
    ret = (format->format == cram);
  }

  return ret;
}

/**
  @brief   Convert the mount path to the source and normalise
  @param   path  Path on mounted FS
  @return  malloc'd pointer to real path

  Note: Should crash out on memory failure
*/
static char* xpath(const char* path) {
  /* Canonicalised source directory and whether it ends with a slash */
  static char* source = NULL;
  static int   src_slashed = 0;

  if (source == NULL) {
    cramp_fuse_t* ctx = CTX;
    source = ctx->conf->source;

    int length = strlen(source);
    src_slashed = ((char)*(source + length - 1) == '/');
  }

  /* Strip leading slashes from mount path */
  char* mount = path;
  while ((char)*mount == '/') {
    ++mount;
  }

  return xasprintf("%s%s%s", source,
                             src_slashed ? "" : "/",
                             mount);
}

/**
  @brief   Initialise filesystem
  @param   conn  FUSE connection info
  @return  ...
*/
void* cramp_init(struct fuse_conn_info* conn) {
  cramp_fuse_t* ctx = CTX;

  /* Log configuration */
  cramp_log("conf.source = %s",      ctx->conf->source);
  cramp_log("conf.debug_level = %d", ctx->conf->debug_level);
  cramp_log("conf.one_thread = %s",  ctx->conf->one_thread ? "true" : "false");

  return ctx;
}

/**
  @brief   Get file attributes
  @param   path   File path
  @param   stbuf  stat buffer
  @return  Exit status (0 = OK; -errno = not so much)
*/
int cramp_getattr(const char* path, struct stat* stbuf) {
  cramp_fuse_t* ctx = CTX;
  char* srcpath = xpath(path);

  int res = lstat(srcpath, stbuf);

  if (res == -1) {
    return -errno;
  }

  free(srcpath);
  return 0;
}

/**
  @brief   Open file
  @param   path  File path
  @param   fi    FUSE file info
  @return  Exit status (0 = OK; -errno = not so much)
*/
int cramp_open(const char* path, struct fuse_file_info* fi) {
  cramp_fuse_t* ctx = CTX;
  char* srcpath = xpath(path);

  (void)fprintf(stderr, "flags: 0x%x\n", fi->flags);

  /* TEST Check CRAM file on open */
  if (possibly_cram(srcpath)) {
    cramp_log("\"%s\" could be a CRAM file...", srcpath);

    /* XXX S_ISREG || S_ISLNK */

    htsFile* fp = hts_open(srcpath, "r");

    if (fp) {
      if (actually_cram(fp)) {
        cramp_log("\"%s\" actually IS a CRAM file :)", srcpath);
      } else {
        cramp_log("Turns out \"%s\" isn't a CRAM file :(", srcpath);
      }

      (void)hts_close(fp);
    }
  }

  int res = open(srcpath, fi->flags);

  if (res == -1) {
    return -errno;
  } else {
    fi->fh = res;
  }

  free(srcpath);
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
  cramp_fuse_t* ctx = CTX;
  char* srcpath = xpath(path);

  DIR* dp;
  struct dirent* de;

  (void)offset;
  (void)fi;

  dp = opendir(srcpath);
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
  free(srcpath);
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
  @param   data  FUSE context
*/
void cramp_destroy(void* data) {
  cramp_fuse_t* ctx = (cramp_fuse_t*)data;
  free(ctx->conf->source);
}
