/* GPLv3 or later
 * Copyright (c) 2015 Genome Research Limited */

#include "config.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "canonicalize.h"
#include "xgetcwd.h"

#include "13amp.h"
#include "cache.h"
#include "fs.h"
#include "log.h"

#include <fuse.h>
#include <fuse_opt.h>

#include <htslib/hts.h>

/* FUSE Operations */
static struct fuse_operations cramp_ops = {
  .init       = cramp_init,
  .getattr    = cramp_getattr,
  .readlink   = cramp_readlink,
  .open       = cramp_open,
  .read       = cramp_read,
  .release    = cramp_release,
  .opendir    = cramp_opendir,
  .readdir    = cramp_readdir,
  .releasedir = cramp_releasedir,
  .destroy    = cramp_destroy
};

/* FUSE Options */
static struct fuse_opt cramp_fuse_opts[] = {
  CRAMP_FUSE_OPT("-S %s",       source, 0),
  CRAMP_FUSE_OPT("--source=%s", source, 0),
  CRAMP_FUSE_OPT("source=%s",   source, 0),

  CRAMP_FUSE_OPT("--cache=%s",  cache,  0),

  FUSE_OPT_KEY("--debug",       CRAMP_FUSE_CONF_KEY_DEBUG_ME),

  FUSE_OPT_KEY("-d",            CRAMP_FUSE_CONF_KEY_DEBUG_ALL),
  FUSE_OPT_KEY("debug",         CRAMP_FUSE_CONF_KEY_DEBUG_ALL),

  FUSE_OPT_KEY("-h",            CRAMP_FUSE_CONF_KEY_HELP),
  FUSE_OPT_KEY("--help",        CRAMP_FUSE_CONF_KEY_HELP),

  FUSE_OPT_KEY("--version",     CRAMP_FUSE_CONF_KEY_VERSION),

  FUSE_OPT_KEY("-f",            CRAMP_FUSE_CONF_KEY_FOREGROUND),

  FUSE_OPT_KEY("-s",            CRAMP_FUSE_CONF_KEY_SINGLETHREAD),

  FUSE_OPT_END
};

/**
  @brief  Return the FUSE version as a string (major.minor)

  Based on StackOverflow answer http://stackoverflow.com/a/31569176
  TODO Use my fuse_pkgversion patch once it's available in FUSE releases
*/
static const char* fuse_pkgversion(void) {
  static char ver[5] = {0, 0, 0, 0, 0};
  
  if (ver[0] == 0) {
    int v = fuse_version();
    int maj = v / 10;
    int min = v % 10;
    (void)snprintf(ver, 5, "%d.%d", maj, min);
  }

  return ver;
}

/**
  @brief  Usage instructions to stderr
  @param  me  Program name (i.e., argv[0])
*/
static void usage(void) {
  /* Secret options!
       --cache=%s  Alternative CRAM stat cache file
       -d          Full debugging messages
       --debug     Just 13 Amp debugging messages (i.e., no FUSE)
       -f          Run in foreground
       -s          Run single threaded                                */

  (void)fprintf(stderr,
    "Usage: %s MOUNTPOINT [OPTIONS]\n"
    "\n"
    "Options:\n"
    "  -S, --source=DIR|URL   Source directory (defaults to the CWD)\n"
    "  -h, --help             This helpful text\n"
    "      --version          Print version\n"
    "\n"
    "The source directory may also be provided as a mount option (e.g., in\n"
    "your fstab). When pointing to a URL, this is expected to resolve to a\n"
    "manifest file (i.e., a file of CRAM URLs).\n"
    "\n"
    "Note: The search order for CRAM reference files is:\n"
    " * Per the REF_CACHE environment variable;\n"
    " * Per the REF_PATH environment variable;\n"
    " * As specified in the CRAM's UR header tag.\n",
  PACKAGE_NAME);
}

/**
  @brief   Parse and set extra-FUSE options
  @param   data     Configuration data
  @param   arg      Argument to parse
  @param   key      Processing key
  @param   outargs  Current argument list
  @return  1 = Keep argument; 0 = Discard argument; -1 = Error

  See documentation for fuse_opt_proc_t (include/fuse_opt.h) for details
*/
static int cramp_fuse_options(void* data, const char* arg, int key, struct fuse_args* outargs) {
  cramp_conf_t* conf = (cramp_conf_t*)data;
  (void)arg;

  switch(key) {
    case CRAMP_FUSE_CONF_KEY_HELP:
      usage();
      exit(1);

    case CRAMP_FUSE_CONF_KEY_VERSION:
      /* Show version information 
         n.b., fuse_version() returns an integer: (maj * 10) + min
               TODO Use my fuse_pkgversion patch once its available   */
      (void)fprintf(stderr,
        "13 Amp %s\n"
        " * HTSLib %s\n"
        " * FUSE %s\n",
      PACKAGE_VERSION, hts_version(), fuse_pkgversion());
      exit(0);

    case CRAMP_FUSE_CONF_KEY_DEBUG_ME:
      conf->debug_level |= DEBUG_ME;
      /* Any debugging => foreground */
      (void)fuse_opt_add_arg(outargs, "-f");
      break;

    case CRAMP_FUSE_CONF_KEY_DEBUG_ALL:
      conf->debug_level |= DEBUG_ALL;
      /* Fall through: debug all => foreground */

    case CRAMP_FUSE_CONF_KEY_FOREGROUND:
      /* -d and -f need to be processed by FUSE */
      return 1;

    case CRAMP_FUSE_CONF_KEY_SINGLETHREAD:
      conf->one_thread = 1;
      /* -s needs to be processed by FUSE */
      return 1;

    default:
      /* Anything not recognised should be processed by FUSE */
      return 1;
  }

  /* Anything that breaks out will not be processed by FUSE */
  return 0;
}

int main(int argc, char** argv) {
  /* Initialise global context */
  static cramp_ctx_t cramp_ctx;
  memset(&cramp_ctx, 0, sizeof(cramp_ctx));
  cramp_ctx_t* ctx = &cramp_ctx;

  /* Initialise settings */
  static cramp_conf_t cramp_conf;
  memset(&cramp_conf, 0, sizeof(cramp_conf));
  ctx->conf = &cramp_conf;

  /* Initialise CRAM stat cache */
  ctx->cache = kh_init(stat_hash);

  /* Parse command line arguments via FUSE */
  struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
  (void)fuse_opt_parse(&args, &cramp_conf, cramp_fuse_opts, cramp_fuse_options);

  /* Sanitise source directory */
  /* TODO URL-based source manifest file */
  if (ctx->conf->source == NULL) {
    /* Default to current working dir */
    ctx->conf->source = xgetcwd();
    if (ctx->conf->source == NULL) {
      WTF("Memory allocation failure");
    }

  } else {
    /* Check path exists as a directory */
    struct stat s;
    int ret = lstat(ctx->conf->source, &s);

    if (ret == -1) {
      if (errno == ENOENT) {
        WTF("\"%s\" does not exist", ctx->conf->source);
      } else {
        WTF("Couldn't stat \"%s\"", ctx->conf->source);
      }

    } else {
      if (!S_ISDIR(s.st_mode)) {
        errno = ENOTDIR;
        WTF("\"%s\" is not a directory", ctx->conf->source);
      }
    }

    /* If we've got this far, we're good. Canonicalise */
    const char* rawsrc = ctx->conf->source;
    ctx->conf->source = canonicalize_filename_mode(rawsrc, CAN_EXISTING);
    free((void*)rawsrc);
  }

  /* Set cache file */
  if (ctx->conf->cache == NULL) {
    ctx->conf->cache = cramp_cache_file(ctx->conf->source);
    if (ctx->conf->cache == NULL) {
      WTF("Memory allocation failure");
    }
  }

  /* Let's go! */
  return fuse_main(args.argc, args.argv, &cramp_ops, &cramp_ctx);
}
