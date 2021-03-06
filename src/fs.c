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
#include "log.h"
#include "util.h"
#include "conv.h"
#include "cache.h"

#include <fuse.h>

#include <htslib/hts.h>
#include <htslib/khash.h>

/* chmod a-w ALL THE THINGS! */
#define UNWRITEABLE (~(S_IWUSR | S_IWGRP | S_IWOTH))

/* Check we have a file or symlink */
#define CAN_OPEN(st_mode) ((st_mode) & (S_IFREG | S_IFLNK))

/* Initialise hash table type */
KHASH_MAP_INIT_STR(hash_t, struct cramp_entry_t*)

/**
  @brief   Initialise filesystem
  @param   conn  FUSE connection info
  @return  Pointer to FUSE global context
*/
void* cramp_init(struct fuse_conn_info* conn) {
  cramp_ctx_t* ctx = CTX;
  (void)conn;

  /* Log configuration */
  LOG("conf.source = %s",      ctx->conf->source);
  LOG("conf.cache = %s",       ctx->conf->cache);
  LOG("conf.bamsize = %s",     human_size(ctx->conf->bamsize));
  LOG("conf.debug_level = %d", ctx->conf->debug_level);
  LOG("conf.one_thread = %s",  ctx->conf->one_thread ? "true" : "false");

  /* Load cache */
  if (cramp_cache_read(ctx->conf->cache, ctx->cache) == -1) {
    /* An inability to read the cache file isn't a fatal error */
    LOG("Couldn't read cache file \"%s\"", ctx->conf->cache); 
  }

  return ctx;
}

/**
  @brief   Get file attributes
  @param   path   File path
  @param   stbuf  stat buffer
  @return  Exit status (0 = OK; -errno = not so much)
*/
int cramp_getattr(const char* path, struct stat* stbuf) {
  cramp_ctx_t* ctx = CTX;

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
        errsav = errno;
        free((void*)srcpath);
        return -errsav;
      }

      /* Inherit stat from CRAM file */
      /* n.b., stat, rather than lstat, to follow symlinks */
      int res = stat(cram_name, stbuf);

      if (res == -1 || !CAN_OPEN(stbuf->st_mode)) {
        /* ...guess not */
        free((void*)srcpath);
        free((void*)cram_name);
        return -errsav;
      }

      /* Set virtual BAM file size */
      (void)cramp_cache_stat(stbuf, cramp_cache_get(ctx->cache, cram_name));
      free((void*)cram_name);

    } else {
      free((void*)srcpath);
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
    int errsav = errno;
    free((void*)srcpath);
    return -errsav;
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
  const char* srcpath = source_path(path);
  if (srcpath == NULL) {
    return -errno;
  }

  struct cramp_filep* f = malloc(sizeof(struct cramp_filep));
  if (f == NULL) {
    free((void*)srcpath);
    return -errno;
  }

  /* Assume we're opening a regular file, forced to read only */
  fi->flags  = O_RDONLY;
  f->type    = fd_normal;
  f->filep   = open(srcpath, fi->flags);
  int errsav = errno;

  if (f->filep == -1) {
    if (errsav == ENOENT && has_extension(srcpath, ".bam")) {
      /* It looks like we might have a virtual BAM file */
      const char* cram_name = sub_extension(srcpath, ".cram");
      free((void*)srcpath);
      if (cram_name == NULL) {
        int errsav = errno;
        free((void*)f);
        return -errsav;
      }

      f->cramp = hts_open(cram_name, "r");
      int cramperr = errno;

      if (f->cramp == NULL) {
        free((void*)cram_name);
        free((void*)f);
        return -cramperr;
      } else {
        const htsFormat* format = hts_get_format(f->cramp);
        if (format->format == cram) {
          /* We've got a genuine CRAM file */
          LOG("Opened virtual BAM file %s from %s", path, cram_name);
          f->type = fd_cram;
        } else {
          (void)hts_close(f->cramp);
          free((void*)cram_name);
          free((void*)f);
          return -errsav;
        }
      }
      free((void*)cram_name);
    } else {
      free((void*)srcpath);
      free((void*)f);
      return -errsav;
    }
  } else {
    free((void*)srcpath);
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
  @return  Exit status (Success: number of bytes read; Fail: -errno)
*/
int cramp_read(const char* path, char* buf, size_t size, off_t offset, struct fuse_file_info* fi) {
  int res = 0;

  struct cramp_filep* f = get_filep(fi);
  (void)path;

  if (f) {
    switch (f->type) {
      case fd_normal:
        if ((res = pread(f->filep, buf, size, offset)) == -1) {
          res = -errno;
        }
        break;

      case fd_cram:
        if ((res = cramp_conv_read(f->cramp, buf, size, offset)) == -1) {
          res = -errno;
        }
        break;

      default:
        res = -EPERM;
    }
  } else {
    res = -EBADF;
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
  int res = 0;

  struct cramp_filep* f = get_filep(fi);
  (void)path;

  if (f) {
    switch (f->type) {
      case fd_normal:
        if (close(f->filep) == -1) {
          res = -errno;
        }
        break;

      case fd_cram:
        if (hts_close(f->cramp) == -1) {
          res = -errno;
        }
        break;

      default:
        res = -EBADF;
    }

    free(f);

  } else {
    res = -EBADF;
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
    res = errno;
    free((void*)srcpath);
    return -res;
  }

  d->dp = opendir(srcpath);
  if (d->dp == NULL) {
    res = errno;
    free((void*)srcpath);
    free((void*)d);
    return -res;
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
  cramp_ctx_t* ctx = CTX;
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
      int errsav = errno;
      free((void*)st);
      errno = errsav;
      goto finish_up;
    }

    struct cramp_entry_t* details = malloc(sizeof(struct cramp_entry_t));
    if (details == NULL) {
      int errsav = errno;
      kh_del(hash_t, contents, key);
      free((void*)st);
      errno = errsav;
      goto finish_up;
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

              if (res < 0) {
                errno = -res;
                free((void*)srcpath);
                free((void*)bam_name);
                goto finish_up;
              }

              if (res) {
                int ret;
                khiter_t key = kh_put(hash_t, contents, bam_name, &ret);

                if (ret == -1) {
                  /* Key insertion failure */
                  free((void*)srcpath);
                  free((void*)bam_name);
                  goto finish_up;
                }

                /* Create virtual entry */
                struct cramp_entry_t* details = malloc(sizeof(struct cramp_entry_t));
                if (details == NULL) {
                  int errsav = errno;
                  kh_del(hash_t, contents, key);
                  free((void*)srcpath);
                  free((void*)bam_name);
                  errno = errsav;
                  goto finish_up;
                }

                details->virtual = 1;
                details->st = calloc(1, sizeof(struct stat));
                memcpy(details->st, st, sizeof(struct stat));

                /* Set virtual BAM file size */
                (void)cramp_cache_stat(details->st, cramp_cache_get(ctx->cache, srcpath));

                /* Insert virtual entry */
                kh_value(contents, key) = details;
              }
              free((void*)srcpath);
            } else {
              free((void*)bam_name);
              goto finish_up;
            } 
          } else {
            free((void*)bam_name);
            goto finish_up;
          }
        } else {
          /* Ignore if there's a clash */
          free((void*)bam_name);
        }
      } else {
        goto finish_up;
      }
    }

    d->entry = NULL;
  }

  /* If we've got this far, then everything's good and we can generate
     the directory entries by ensuring the error number is zero. Then,
     or otherwise, we free memory and destroy the hash table.         */
  errno = 0;

finish_up:;
  int errsav = errno;

  const char* entry;
  struct cramp_entry_t* details;
  kh_foreach(contents, entry, details, {
    /* Create directory entry */
    if (!errsav && filler(buf, entry, details->st, 0)) {
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

  return -errsav;
}

/**
  @brief   Release an open directory
  @param   path  File path
  @param   fi    FUSE file info
  @return  Exit status (0 = OK; -errno = not so much)
*/
int cramp_releasedir(const char* path, struct fuse_file_info* fi) {
  int res = 0;

  struct cramp_dirp* d = get_dirp(fi);
  (void)path;

  if(closedir(d->dp) == -1) {
    res = -errno;
  }
  free((void*)d);

  return res;
}

/**
  @brief   Clean up filesystem on exit
  @param   data  FUSE context
*/
void cramp_destroy(void* data) {
  cramp_ctx_t* ctx = (cramp_ctx_t*)data;

  if (cramp_cache_write(ctx->conf->cache, ctx->cache) == -1) {
    LOG("Couldn't write to cache file \"%s\"", ctx->conf->cache);
  }

  cramp_cache_destroy(ctx->cache);
  free((void*)ctx->conf->source);
  free((void*)ctx->conf->cache);
}
