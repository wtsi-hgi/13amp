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
#include "xvasprintf.h"

#include "13amp.h"
#include "13amp_log.h"

#include <fuse.h>

#include <htslib/hts.h>
#include <htslib/khash.h>

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

/**
  @brief   Cast the file handle to the directory structure
  @param   FUSE file info

  The FUSE file info has a file handle member, typed as unsigned long.
  To extend this, we cast in a pointer to a cramp_dirp structure. This
  function casts it back, so we can use it.
*/
static struct cramp_dirp* get_dirp(struct fuse_file_info* fi) {
  return (struct cramp_dirp*)(uintptr_t)fi->fh;
}

/**
  @brief   Directory entry structure
  @var     st       Stat structure of entry
  @var     virtual  Entry is virtual (0 = False; 1 = True)
*/
struct cramp_entry_t {
  struct stat* st;
  int          virtual;
};

/* Initialise hash table type */
KHASH_MAP_INIT_STR(hash_t, struct cramp_entry_t*)

enum fpType { fpNorm, fpCRAM };

/**
  @brief   File structure
  @var     path   File path
  @var     kind   Union tag
  @var     filep  Normal file handle
  @var     cramp  CRAM file handle

  TODO What else needs to go in here?...
*/
struct cramp_filep {
  const char* path;
  enum fpType kind;
  union {
    int       filep;
    htsFile*  cramp;
  };
};

/**
  @brief   Cast the file handle to the file structure
  @param   FUSE file info

  The FUSE file info has a file handle member, typed as unsigned long.
  To extend this, we cast in a pointer to a cramp_filep structure. This
  function casts it back, so we can use it.
*/
static struct cramp_filep* get_filep(struct fuse_file_info* fi) {
  return (struct cramp_filep*)(uintptr_t)fi->fh;
}

/**
  @brief   Check file extension for .cram
  @param   path  File path
  @return  1 = Yep; 0 = Nope

  This is just a stupid-simple string comparison. It needs to be cheap,
  as it will be run regularly (e.g., on every entry in a readdir), but
  still provide a reasonable proxy to the truth.
*/
static inline int possibly_cram(const char* path) {
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
static inline int actually_cram(htsFile* fp) {
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
static inline char* xapath(const char* path) {
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

    /* ENOENT => virtual file */
    if (errno == ENOENT) {
      memset(stbuf, 0, sizeof(struct stat));
      stbuf->st_mode = S_IFREG | 0444;
      stbuf->st_nlink = 1;
      stbuf->st_size = 1;
      return 0;
    }

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
  int res;

  char* srcpath = xapath(path);
  if (srcpath == NULL) {
    return -errno;
  }

  struct cramp_filep* f = malloc(sizeof(struct cramp_filep));
  if (f == NULL) {
    return -errno;
  }

  f->path = srcpath;
  int opened = 0;

  /* Check we've got a CRAM file */
  if (possibly_cram(f->path)) {
    if (1 /* TODO S_ISREG(f->path) || S_ISLNK(f->path) */) {
      f->cramp = hts_open(f->path, "r");
      
      if (f->cramp == NULL) {
        res = -errno;
        free(f);
        return res;
      } else {
        if (actually_cram(f->cramp)) {
          /* We've got a CRAM file */
          f->kind = fpCRAM;
          opened  = 1;
        } else {
          if (hts_close(f->cramp) == -1) {
            return -errno;
          }
        }
      }
    }
  }

  /* Regular file */
  if (!opened) {
    f->filep = open(f->path, fi->flags);

    if (f->filep == -1) {
      res = -errno;
      free(f);
      return res;
    } else {
      f->kind = fpNorm;
      opened  = 1;
    }
  }

  fi->fh = (unsigned long)f;
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

  struct cramp_filep* f = get_filep(fi);
  (void)path;

  if (f) {
    switch (f->kind) {
      case fpNorm:
        res = pread(f->filep, buf, size, offset);
        if (res == -1) {
          return -errno;
        }
        break;

      case fpCRAM:
        /* TODO */
        cramp_log("CRAM file %s", f->path);
        break;

      default:
        return -EPERM;
    }
  } else {
    return -EBADF;
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
  int res;

  struct cramp_filep* f = get_filep(fi);
  (void)path;

  if (f) {
    free((void*)(f->path));

    switch (f->kind) {
      case fpNorm:
        if (close(f->filep) == -1) {
          res = -errno;
        }
        break;

      case fpCRAM:
        if (hts_close(f->cramp) == -1) {
          res = -errno;
        }
        break;

      default:
        return -EBADF;
    }

    free(f);

  } else {
    return -EBADF;
  }

  return res;
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
  free(srcpath);
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
  khash_t(hash_t) *contents = kh_init(hash_t);

  /* TODO This function is really messy. Tidy it up!! */

  /* Seek to the correct offset, if necessary */
  if (offset != d->offset) {
    seekdir(d->dp, offset);
    d->entry = NULL;
    d->offset = offset;
  }

  /* Loop through directory contents */
  while (1) {
    /* Read directory, if necessary; break loop when nothing returned */
    if (!d->entry) {
      d->entry = readdir(d->dp);
      if (!d->entry) {
        break;
      }
    }

    struct stat* st = calloc(1, sizeof(struct stat));
    if (st == NULL) {
      return -errno;
    }

    st->st_ino  = d->entry->d_ino;
    st->st_mode = DTTOIF(d->entry->d_type) & UNWRITEABLE; 

    /* Insert item into hash table */
    int ret;
    khiter_t key = kh_put(hash_t, contents, d->entry->d_name, &ret);
    if (ret == -1) {
      return -errno;
    }

    struct cramp_entry_t* details = malloc(sizeof(struct cramp_entry_t));
    if (details == NULL) {
      return -errno;
    }

    details->st = st;
    details->virtual = 0;
    kh_value(contents, key) = details;

    /* Inject virtual BAM file, if we've got a CRAM and no clash */
    if ((S_ISREG(st->st_mode) || S_ISLNK(st->st_mode)) &&
        possibly_cram(d->entry->d_name)) {

      /* Full path of directory entry */
      int length = strlen(path);
      int path_slashed = ((char)*(path + length - 1) == '/');
      const char* fullpath = xasprintf("%s%s%s", path, 
                                                 path_slashed ? "" : "/",
                                                 d->entry->d_name);
      if (fullpath == NULL) {
        return -errno;
      }
    
      char* srcpath = xapath(fullpath);
      if (srcpath == NULL) {
        return -errno;
      }

      /* Have we *actually* got a CRAM? */
      htsFile* fp = hts_open(srcpath, "r");
      if (fp == NULL) {
        return -errno;
      }

      if (actually_cram(fp)) {
        /* Entity name: s/\.cram$/.bam */
        size_t bam_length = strlen(d->entry->d_name) - 1;
        const char* bam_name = malloc(bam_length * sizeof(char));
        if (bam_name == NULL) {
          return -errno;
        }

        memcpy((void*)bam_name, d->entry->d_name, bam_length);
        memcpy((void*)bam_name + bam_length - 4, ".bam", 5);

        int ret;
        khiter_t key = kh_put(hash_t, contents, bam_name, &ret);

        switch (ret) {
          case -1:
            return -errno;

          case 0:
            /* Ignore clash */
            break;

          case 1:
          case 2: {
            /* Create virtual entry */
            struct cramp_entry_t* details = malloc(sizeof(struct cramp_entry_t));
            if (details == NULL) {
              return -errno;
            }

            details->virtual = 1;
            details->st = calloc(1, sizeof(struct stat));
            memcpy(details->st, st, sizeof(struct stat));

            kh_value(contents, key) = details;
            break;
          }

          default:
            break;
        }
      }

      if(hts_close(fp) == -1) {
        return -errno;
      }
      free(srcpath);
      free((void*)fullpath);
    }

    d->entry = NULL;
  }

  /* Iterate through hash to populate directory */
  const char* entry;
  struct cramp_entry_t* details;

  kh_foreach(contents, entry, details, {
    /* Create directory entry */
    if (filler(buf, entry, details->st, 0)) {
      break;
    }

    /* Free entry data */
    if (details->virtual) {
      free((void*)entry);
    }
    free(details->st);
    free(details);
  })

  kh_destroy(hash_t, contents);
  return 0;
}

/**
  @brief   Release an open directory
  @param   path  File path
  @param   fi    FUSE file info
  @return  Exit status (0 = OK; -errno = not so much)
*/
int cramp_releasedir(const char* path, struct fuse_file_info* fi) {
  int res;

  struct cramp_dirp* d = get_dirp(fi);
  (void)path;

  if(closedir(d->dp) == -1) {
    res = -errno;
  }
  free(d);

  return res;
}

/**
  @brief   Clean up filesystem on exit
  @param   data  FUSE context
*/
void cramp_destroy(void* data) {
  cramp_fuse_t* ctx = (cramp_fuse_t*)data;
  free(ctx->conf->source);
}
