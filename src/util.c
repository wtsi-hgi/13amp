/* GPLv3 or later
 * Copyright (c) 2015 Genome Research Limited */

#include "config.h"

#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "main.h"
#include "util.h"

#include <fuse.h>

#include <htslib/hts.h>

/**
  @brief   Concatenate two path strings
  @param   path1  First file path
  @param   path2  Second file path
  @return  malloc'd pointer to concatenation
*/
const char* path_concat(const char* path1, const char* path2) {
  /* Determine length of path1, minus trailing slashes */
  ssize_t len1 = (ssize_t)strlen(path1);
  while (*(path1 + --len1) == '/') { if (len1 < 0) break; }
  ++len1;

  /* Find start of path2, minus initial slashes */
  char* stripped = (char*)path2;
  while (*(stripped++) == '/') { continue; }
  --stripped;
  size_t len2 = strlen(stripped);

  /* Allocate and construct output */
  const char* output = malloc(len1 + len2 + 2);
  if (output) {
    memcpy((void*)output, path1, len1);
    memset((void*)(output + len1), '/', 1);
    memcpy((void*)(output + len1 + 1), stripped, len2 + 1);
  }

  return output;
}

/**
  @brief   Convert the mount path to the source and normalise
  @param   path  Path on mounted FS
  @return  malloc'd pointer to real path
*/
const char* source_path(const char* path) {
  static const char* source = NULL;

  if (source == NULL) {
    cramp_fuse_t* ctx = CTX;
    source = ctx->conf->source;
  }

  return path_concat(source, path);
}

/**
  @brief   Check if a path name ends with a specified extension
  @param   path  File path
  @param   ext   Extension string (including "."; e.g., ".cram")
  @return  1 = Yep; 0 = Nope

  This is just a simple string comparison.
*/
int has_extension(const char* path, const char* ext) {
  const char* extFrom = strrchr(path, '.');
  return extFrom && !strcmp(extFrom, ext);
}

/**
  @brief   Substitute the file path extension with something else
  @param   path  File path
  @param   ext   New extension string (including "."; e.g., ".cram")
  @return  malloc'd pointer to substituted path string

  Note: If no file extension is found, then it's just appended:
    sub_extension(file.foo, bar) => file.bar
    sub_extension(file,     bar) => file.bar
*/
const char* sub_extension(const char* path, const char* ext) {
  const char* extFrom = strrchr(path, '.');

  size_t lenext = strlen(ext);
  size_t offset = extFrom ? extFrom - path : strlen(path);

  const char* output = malloc(offset + lenext + 1);
  if (output) {
    memcpy((void*)output, path, offset);
    memcpy((void*)(output + offset), ext, lenext + 1);
  }

  return output;
}

/**
  @brief   Check we have a CRAM file
  @param   path  File path
  @return  1 = Yep; 0 = Nope; -errno = Error

  Note: This doesn't check that the path is a regular file/symlink, that
  should be done in advance.
*/
int is_cram(const char* path) {
  int ret = 0;

  /* Use HTSLib to open the file and check its format */
  htsFile* fp = hts_open(path, "r");
  if (fp) {
    const htsFormat* format = hts_get_format(fp);
    if (format) {
      ret = (format->format == cram);
    } else {
      ret = -errno;
    }
    if (hts_close(fp) == -1) {
      ret = -errno;
    }
  } else {
    ret = -errno;
  }

  return ret;
}

/**
  @brief   Cast the file handle to the directory structure
  @param   FUSE file info

  The FUSE file info has a file handle member, typed as unsigned long.
  To extend this, we cast in a pointer to a cramp_dirp structure. This
  function casts it back, so we can use it.
*/
struct cramp_dirp* get_dirp(struct fuse_file_info* fi) {
  return (struct cramp_dirp*)(uintptr_t)fi->fh;
}

/**
  @brief   Cast the file handle to the file structure
  @param   FUSE file info

  The FUSE file info has a file handle member, typed as unsigned long.
  To extend this, we cast in a pointer to a cramp_filep structure. This
  function casts it back, so we can use it.
*/
struct cramp_filep* get_filep(struct fuse_file_info* fi) {
  return (struct cramp_filep*)(uintptr_t)fi->fh;
}
