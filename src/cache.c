/* GPLv3 or later
 * Copyright (c) 2015 Genome Research Limited */

#include "config.h"

#include <errno.h>
#include <limits.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "13amp.h"
#include "cache.h"
#include "log.h"
#include "util.h"

/**
  @brief   Put record into the cache by source
  @param   cache   CRAM stat cache
  @param   source  CRAM file
  @param   record  Pointer to record data
  @return  1 = Success; 0 = Fail
*/
int cramp_cache_put(cramp_cache_t* cache, const char* source, cramp_stat_t* record) {
  int ret;

  /* Check key doesn't exist */
  khiter_t key = kh_get(stat_hash, cache, source);
  if (key != kh_end(cache)) {
    return 0;
  }

  /* Insert key */
  key = kh_put(stat_hash, cache, source, &ret);
  if (ret == -1) {
    return 0;
  }

  /* Set value */
  kh_value(cache, key) = record;
  return 1;
}

/**
  @brief   Get record from cache by source
  @param   cache   CRAM stat cache
  @param   source  CRAM file
  @return  Pointer to record (NULL on failure)
*/
cramp_stat_t* cramp_cache_get(cramp_cache_t* cache, const char* source) {
  khiter_t key = kh_get(stat_hash, cache, source);
  if (key == kh_end(cache)) {
    return NULL;
  }

  return kh_value(cache, key);
}

/**
  @brief   Free all memory allocated by stat cache
  @param   cache  CRAM stat cache
*/
void cramp_cache_destroy(cramp_cache_t* cache) {
  const char* source;
  cramp_stat_t* record;
  kh_foreach(cache, source, record, {
    free((void*)source);
    free((void*)record);
  })
  kh_destroy(stat_hash, cache);
}

/**
  @brief   Set the file size based on cached value
  @param   stbuf   Pointer to stat structure
  @param   cached  Pointer to cache stat structure
  @return  Pointer to stat structure

  If uncached, zero sized, or out of date, then the size is set per the
  configuration (default SSIZE_MAX); otherwise, it is set appropriately.
*/
struct stat* cramp_cache_stat(struct stat* stbuf, cramp_stat_t* cached) {
  static off_t bamsize = 0;

  if (bamsize == 0) {
   cramp_ctx_t* ctx = CTX;
   bamsize = ctx->conf->bamsize;
  }

  if (cached == NULL || cached->size == 0 || cached->mtime < stbuf->st_mtime) {
    stbuf->st_size = bamsize;
  } else {
    stbuf->st_size = cached->size;
  }
  
  return stbuf;
}

/**
  @brief   djb2 Hash

  From http://www.cse.yorku.ca/~oz/hash.html
*/
unsigned long hash(unsigned char* str) {
  unsigned long hash = 5381;
  int c;

  while ((c = *str++)) {
    hash = ((hash << 5) + hash) + c;
  }

  return hash;
}

/**
  @brief   Determine and initialise the cache directory
  @return  malloc'd pointer to cache directory

  The cache directory will be read from the CRAMP_CACHE environment
  variable, defaulting to ~/.cache/13amp if that isn't set. If the home
  cache directory doesn't exist, it will be created.
*/
const char* cache_dir_init(void) {
  const char* cachedir = NULL;

  char* CRAMP_CACHE = getenv("CRAMP_CACHE");
  if (CRAMP_CACHE) {
    /* Copy environment variable, so we have consistent state to free */
    size_t len = strlen(CRAMP_CACHE);
    cachedir = malloc(len + 1);
    if (cachedir == NULL) {
      return NULL;
    }
    memcpy((void*)cachedir, CRAMP_CACHE, len + 1);

  } else {
    /* Default to ~/.cache/13amp */
    const char* homedir = getenv("HOME");
    if (homedir == NULL) {
      homedir = getpwuid(getuid())->pw_dir;
    }

    const char* cachepath[2];
    cachepath[0] = path_concat(homedir, ".cache");
    if (cachepath[0] == NULL) {
      return NULL;
    }
    cachedir = cachepath[1] = path_concat(cachepath[0], "13amp");
    if (cachedir == NULL) {
      return NULL;
    }

    /* Create directories, if necessary */
    for (ssize_t i = 0; i < 2; ++i) {
      struct stat st;

      if (lstat(cachepath[i], &st) == -1) {
        if (errno == ENOENT) {
          if (mkdir(cachepath[i], S_IRWXU | S_IRWXG | S_IRWXO) == -1) {
            WTF("Cannot create cache directory \"%s\"", cachedir);
          }
        } else {
          WTF("Cannot create cache directory: Couldn't stat \"%s\"", cachepath[i]);
        }
      } else {
        if (!S_ISDIR(st.st_mode)) {
          errno = ENOTDIR;
          WTF("Cannot create cache directory: \"%s\" exists, but is not a directory", cachepath[i]);
        }
      }
    }

    free((void*)cachepath[0]);
  }

  return cachedir;
}

/**
  @brief   Generate the cache file path
  @param   source  Source directory
  @return  malloc'd pointer to cache file path

  The cache file name is the hash of the source directory (or URL),
  concatenated to the cache directory.

  FIXME Better choice of hash function. MD5?...
*/
const char* cramp_cache_file(const char* source) {
  unsigned long srchash = hash((unsigned char*)source);
  size_t len = sizeof(srchash);

  /* Cache file name: Hex of hash of source directory path */
  char* cachefile = malloc(len + 1);
  if (cachefile == NULL) {
    return NULL;
  }
  (void)snprintf(cachefile, len, "%0*lx", (int)len, srchash);

  /* Initialise cache directory */
  const char* cachedir = cache_dir_init();
  if (cachedir == NULL) {
    return NULL;
  }

  const char* fq = path_concat(cachedir, cachefile);

  free((void*)cachedir);
  free((void*)cachefile);

  return fq;
}

/**
  The CRAM stat cache file is a simple, plain-text, one record per line
  format using ":" characters as field delimiters. The fields will be in
  the following fixed order:

  * CRAM source path
  * CRAM mtime (seconds since UNIX epoch)
  * Converted BAM size (bytes)
  
  ...with potentially more to follow. For example:

    /path/to/a/file.cram:1370220400:12345676890
    /yet/another/example.cram:1413154800:9876543210

  Lines beginning with a "#" are comments. By convention, the first line
  will be a comment that specifies the source directory/URL, as a kind
  of pseudo-header.
*/

/**
  @brief   CRAM stat cache file field index
*/
enum chunk {
  CHUNK_SOURCE  = 0,
  CHUNK_MTIME   = 1,
  CHUNK_BAMSIZE = 2,

  /* Define this to be 1 + the last field index */
  /* i.e., The number of fields in each record */
  CHUNK_EOF     = 3
};

/**
  @brief   Read the CRAM stat cache from disk
  @param   path   Path to the cache file
  @param   cache  Pointer to the stat cache hash
  @return  Non-negative: Number of entries read; -1: Error

  n.b., If the cache file doesn't exist, this will return 0
*/
ssize_t cramp_cache_read(const char* path, cramp_cache_t* cache) {
  static const int ALL_FOUND = (1 << CHUNK_EOF) - 1;

  ssize_t read = 0;
  FILE* file = fopen(path, "r+");
  if (file == NULL) {
    return -1;
  }

  char* line = malloc(LINE_MAX + 1);
  if (line == NULL) {
    return -1;
  }

  /* Read file line-by-line */
  while (fgets(line, LINE_MAX + 1, file) != NULL) {
    const char* p = line;
    int success = 0;

    /* Strip leading whitespace */
    while (*p) {
      if (*p != ' ' && *p != '\t' && *p != '\n') {
        break;
      }
      ++p;
    }

    /* Ignore comments and blank lines */
    if (*p == '#' || *p == '\0') {
      continue;
    }

    /* Chunk by delimiter */
    const char* start = p;
    const char* end;
    enum chunk  field = CHUNK_SOURCE;

    int found = 0;
    const char*   source = NULL;
    cramp_stat_t* record = malloc(sizeof(cramp_stat_t));
    if (record == NULL) {
      return -1;
    }

    /* Read line character-by-character until a delimiter is hit */
    while (*p) {
      if (*p == ':' || *p == '\n' || *p == '\0') {
        int isgood  = 0;
        int dealloc = 1;
        end = p;

        /* Copy data chunk */
        size_t chunklen = end - start;
        const char* data = malloc(chunklen + 1);
        memcpy((void*)data, start, chunklen);
        memset((void*)(data + chunklen), '\0', 1);

        /* Assign chunk, if possible */
        switch (field) {
          case CHUNK_SOURCE:
            isgood  = 1;
            dealloc = 0;
            source  = data;
            break;

          case CHUNK_MTIME:
            isgood = sscanf(data, "%ld", &(record->mtime));
            break;

          case CHUNK_BAMSIZE:
            isgood = sscanf(data, "%lld", &(record->size));
            break;

          default:
            /* Ignore / reserve for future use */
            break;
        }

        if (isgood) {
          found |= (1 << field);
        }
        if (dealloc || !isgood) {
          free((void*)data);
        }

        start = ++end;
        ++field;
      }
      ++p;
    }

    /* Insert record into cache */
    if (found == ALL_FOUND) {
      int ret;
      khiter_t newKey = kh_put(stat_hash, cache, source, &ret);
      if (ret != -1) {
        kh_value(cache, newKey) = record;
        ++read;
        success = 1;
      }
    }

    /* Free everything that needs to be on parse failure */
    if (!success) {
      free((void*)source);
      free((void*)record);
    }
  }

  free((void*)line);
  (void)fclose(file);

  LOG("Read %ld entries from cache", read);
  return read;
}

/**
  @brief   Write the CRAM stat cache to disk
  @param   path    Path to the cache file
  @param   cache   Pointer to the stat cache hash
  @return  Non-negative: Number of entries written; -1: Error
*/
ssize_t cramp_cache_write(const char* path, cramp_cache_t* cache) {
  /* Open cache file */
  FILE* output = fopen(path, "w+");
  if (output == NULL) {
    return -1;
  }

  /* Write pseudo-header */
  cramp_ctx_t* ctx = CTX;
  const char*  source = ctx->conf->source;
  (void)fprintf(output, "# %s\n", source);
  
  /* Write data */
  ssize_t       written = 0;
  const char*   cramfile;
  cramp_stat_t* record;
  kh_foreach(cache, cramfile, record, {
    (void)fprintf(output, "%s:%ld:%lld\n", cramfile,
                                           record->mtime,
                                           record->size);
    ++written;
  })

  (void)fclose(output);

  LOG("Wrote %ld entries to cache", written);
  return written;
}
