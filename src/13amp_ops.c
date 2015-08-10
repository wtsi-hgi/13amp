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

#include "13amp.h"
#include "13amp_log.h"
#include "13amp_util.h"

#include <fuse.h>

#include <htslib/hts.h>
#include <htslib/khash.h>

/* chmod a-w ALL THE THINGS! */
#define UNWRITEABLE (~(S_IWUSR | S_IWGRP | S_IWOTH))

/* Check we have a file or symlink */
#define CAN_OPEN(st_mode) (S_ISREG(st_mode) || S_ISLNK(st_mode))

/* Initialise hash table type */
KHASH_MAP_INIT_STR(hash_t, struct cramp_entry_t*)

/**
  @brief   Initialise filesystem
  @param   conn  FUSE connection info
  @return  Pointer to FUSE global context
*/
void* cramp_init(struct fuse_conn_info* conn) {
  cramp_fuse_t* ctx = CTX;

  /* Log configuration */
  LOG("conf.source = %s",      ctx->conf->source);
  LOG("conf.debug_level = %d", ctx->conf->debug_level);
  LOG("conf.one_thread = %s",  ctx->conf->one_thread ? "true" : "false");

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

  const char* srcpath = source_path(path);
  if (srcpath == NULL) {
    return -errno;
  }

  int res = lstat(srcpath, stbuf);
  int errsav = errno;

  if (res == -1) {
    if (errsav == ENOENT && has_extension(srcpath, ".bam")) {
      /* It looks like we might have a virtual BAM file */
      const char* cram_name = sub_extension(srcpath, ".cram");
      if (cram_name == NULL) {
        return -errno;
      }

      /* Inherit stat from CRAM file */
      /* n.b., stat, rather than lstat, to follow symlinks */
      int res = stat(cram_name, stbuf);
      free((void*)cram_name);

      if (res == -1 || !CAN_OPEN(stbuf->st_mode)) {
        /* ...guess not */
        return -errsav;
      }

      /* Virtual BAM files should have zero size */
      stbuf->st_size = 0;

    } else {
      return -errsav;
    }
  }

  /* Make read only */
  stbuf->st_mode &= UNWRITEABLE;

  free((void*)srcpath);
  return 0;
}

/**
  @brief   Get symlink target
  @param   path  File path
  @param   buf   Symlink target buffer
  @param   size  Length of buffer
*/
int cramp_readlink(const char* path, char* buf, size_t size) {
  int res;

  const char *srcpath = source_path(path);
  if (srcpath == NULL) {
    return -errno;
  }

  memset(buf, 0, size);
  res = readlink(srcpath, buf, size);
  if (res == -1) {
    return -errno;
  }

  free((void*)srcpath);
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

  const char* srcpath = source_path(path);
  if (srcpath == NULL) {
    return -errno;
  }

  struct cramp_filep* f = malloc(sizeof(struct cramp_filep));
  if (f == NULL) {
    return -errno;
  }

  f->path = srcpath;
  int opened = 0;
  
#if 0
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
          opened = 1;
        } else {
          if (hts_close(f->cramp) == -1) {
            return -errno;
          }
        }
      }
    }
  }
#endif

  /* Regular file */
  if (!opened) {
    f->filep = open(f->path, fi->flags);

    if (f->filep == -1) {
      res = -errno;
      free(f);
      return res;
    } else {
      f->kind = fpNorm;
      opened = 1;
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
        LOG("CRAM file %s", f->path);
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

  const char* srcpath = source_path(path);
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
  free((void*)srcpath);
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

    /* If the key already exists in the hash table, then it MUST be an
       injected virtual BAM file. Our policy is that real files mask
       virtual ones, so we must remove the injected entry and free its
       memory allocation (rather than simply overwriting).            */

    khiter_t key = kh_get(hash_t, contents, d->entry->d_name);
    if (key != kh_end(contents)) {
      const char* clash_key = kh_key(contents, key);
      struct cramp_entry_t* clash_details = kh_value(contents, key);
      free((void*)clash_key);
      free((void*)clash_details->st);
      free((void*)clash_details);
      kh_del(hash_t, contents, key);
    }

    /* Insert item into hash table */
    int ret;
    key = kh_put(hash_t, contents, d->entry->d_name, &ret);
    if (ret == -1) {
      return -errno;
    }

    struct cramp_entry_t* details = malloc(sizeof(struct cramp_entry_t));
    if (details == NULL) {
      return -errno;
    }

    details->virtual = 0;
    details->st = st;
    kh_value(contents, key) = details;

    /* Inject virtual BAM file, if we've got a non-clashing CRAM

       1. Check we've got a file/symlink
       2. Check extension is ".cram"
       3. Check there isn't a clashing ".bam"
       4. Check it is actually a CRAM
       5. Inject :)                                                   */

    if (CAN_OPEN(st->st_mode) && has_extension(d->entry->d_name, ".cram")) {
      const char* bam_name = sub_extension(d->entry->d_name, ".bam");
      if (bam_name) {
        if (kh_get(hash_t, contents, bam_name) == kh_end(contents)) {
          const char* fullpath = path_concat(path, d->entry->d_name);
          if (fullpath) {
            const char* srcpath = source_path(fullpath);
            free((void*)fullpath);
            if (srcpath) {
              int res = is_cram(srcpath);
              free((void*)srcpath);

              if (res < 0) {
                return res;
              }

              if (res) {
                int ret;
                khiter_t key = kh_put(hash_t, contents, bam_name, &ret);

                if (key == -1) {
                  /* Key insertion failure */
                  return -errno;
                }

                /* Create virtual entry */
                struct cramp_entry_t* details = malloc(sizeof(struct cramp_entry_t));
                if (details == NULL) {
                  return -errno;
                }

                details->virtual = 1;
                details->st = calloc(1, sizeof(struct stat));
                memcpy(details->st, st, sizeof(struct stat));

                /* Virtual BAM files should have zero size */
                details->st->st_size = 0;

                /* Insert virtual entry */
                kh_value(contents, key) = details;
              }
            } else {
              return -errno;
            } 
          } else {
            return -errno;
          }
        } else {
          /* Ignore if there's a clash */
          free((void*)bam_name);
        }
      } else {
        return -errno;
      }
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

    /* Free hash table entry data */
    if (details->virtual) {
      free((void*)entry);
    }
    free((void*)details->st);
    free((void*)details);
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
