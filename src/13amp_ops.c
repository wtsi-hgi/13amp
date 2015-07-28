/* GPLv3 or later
 * Copyright (c) 2015 Genome Research Limited */

/* FUSE operations derived from fusexmp.c and fusexmp_fh.c
 * Copyright (c) 2001-2007 Miklos Szeredi */

#include "config.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "gl_avltreehash_list.h"
#include "hash-pjw.h"
#include "xvasprintf.h"

#include "13amp.h"
#include "13amp_log.h"

#include <fuse.h>

#include <htslib/hts.h>

/* Get context macro */
#define CTX (cramp_fuse_t*)(fuse_get_context()->private_data)

/* chmod a-w ALL THE THINGS! */
#define UNWRITEABLE (~(S_IWUSR | S_IWGRP | S_IWOTH))

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

static struct cramp_dirp* get_dirp(struct fuse_file_info* fi) {
  return (struct cramp_dirp*)(uintptr_t)fi->fh;
}

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

  Note: This will call the `xalloc_die` stub when there's a memory
  allocation failure; but we want FUSE to handle such cases, per its
  design, so we still have to do manual checking.
*/
static char* xapath(const char* path) {
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
  const char* mount = path;
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
  @return  Pointer to FUSE global context
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

  char* srcpath = xapath(path);
  if (srcpath == NULL) {
    return -errno;
  }

  int res = lstat(srcpath, stbuf);

  if (res == -1) {
    return -errno;
  }

  /* Make read only */
  stbuf->st_mode &= UNWRITEABLE;

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

  char* srcpath = xapath(path);
  if (srcpath == NULL) {
    return -errno;
  }

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
  int res;

  char* srcpath = xapath(path);
  if (srcpath == NULL) {
    return -errno;
  }

  struct cramp_dirp* d = malloc(sizeof(struct cramp_dirp));
  if (d == NULL) {
    return -errno;
  }

  d->dp = opendir(srcpath);
  if (d->dp == NULL) {
    res = -errno;
    free(d);
    return res;
  }

  d->offset = 0;
  d->entry = NULL;

  fi->fh = (unsigned long)d;
  return 0;
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
  struct cramp_dirp* d = get_dirp(fi);
  (void)path;

  if (offset != d->offset) {
    seekdir(d->dp, offset);
    d->entry = NULL;
    d->offset = offset;
  }

  while (1) {
    struct stat st;
    off_t nextoff;

    if (!d->entry) {
      d->entry = readdir(d->dp);
      if (!d->entry) {
        break;
      }
    }

    memset(&st, 0, sizeof(st));

    st.st_ino = d->entry->d_ino;
    st.st_mode = DTTOIF(d->entry->d_type) & UNWRITEABLE;

    nextoff = telldir(d->dp);

    if (filler(buf, d->entry->d_name, &st, nextoff)) {
      break;
    }

    d->entry = NULL;
    d->offset = nextoff;
  }

  return 0;
}

/**
  @brief   Release an open directory
  @param   path  File path
  @param   fi    FUSE file info
  @return  Exit status (0 = OK; -errno = not so much)
*/
int cramp_releasedir(const char* path, struct fuse_file_info* fi) {
  struct cramp_dirp* d = get_dirp(fi);
  (void)path;

  closedir(d->dp);
  free(d);

  return 0;
}

/**
  @brief   Clean up filesystem on exit
  @param   data  FUSE context
*/
void cramp_destroy(void* data) {
  cramp_fuse_t* ctx = (cramp_fuse_t*)data;
  free(ctx->conf->source);
}
