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
#include "xgetcwd.h"

#include "13amp.h"
#include "13amp_ops.h"
#include "13amp_log.h"

#include <fuse.h>
#include <fuse_opt.h>

#include <htslib/hts.h>

/* FUSE Operations */
static struct fuse_operations cramp_ops = {
  .init    = cramp_init,
  .getattr = cramp_getattr,
  .open    = cramp_open,
  .read    = cramp_read,
  .release = cramp_release,
  /* .opendir = cramp_opendir, */
  .readdir = cramp_readdir,
  /* .releasedir = cramp_releasedir, */
  .destroy = cramp_destroy
};

/* FUSE Options */
static struct fuse_opt cramp_fuse_opts[] = {
  CRAMP_FUSE_OPT("-S %s",       source, 0),
  CRAMP_FUSE_OPT("--source=%s", source, 0),
  CRAMP_FUSE_OPT("source=%s",   source, 0),

  FUSE_OPT_KEY("--debug",       CRAMP_FUSE_CONF_KEY_DEBUG_ME),

  FUSE_OPT_KEY("-d",            CRAMP_FUSE_CONF_KEY_DEBUG_ALL),
  FUSE_OPT_KEY("debug",         CRAMP_FUSE_CONF_KEY_DEBUG_ALL),

  FUSE_OPT_KEY("-V",            CRAMP_FUSE_CONF_KEY_VERSION),
  FUSE_OPT_KEY("--version",     CRAMP_FUSE_CONF_KEY_VERSION),

  FUSE_OPT_KEY("-h",            CRAMP_FUSE_CONF_KEY_HELP),
  FUSE_OPT_KEY("--help",        CRAMP_FUSE_CONF_KEY_HELP),

  FUSE_OPT_KEY("-f",            CRAMP_FUSE_CONF_KEY_FOREGROUND),

  FUSE_OPT_KEY("-s",            CRAMP_FUSE_CONF_KEY_SINGLETHREAD),

  FUSE_OPT_END
};

/**
  @brief  Return the FUSE version as a string (major.minor)

  Based on StackOverflow answer http://stackoverflow.com/a/31569176/876937
*/
static char* str_fuse_version(void) {
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
static void usage(char* me) {
  /* Secret options!
       -d       Full debugging messages
       --debug  Just 13 Amp debugging messages (i.e., no FUSE)
       -f       Run in foreground
       -s       Run single threaded                                   */

  (void)fprintf(stderr,
    "Usage: %s mountpoint [options]\n"
    "\n"
    "Options:\n"
    "  -S   --source   [DIR | URL]  Source directory (defaults to the PWD)\n"
    "  -V   --version               Print version\n"
    "  -h   --help                  This helpful text\n"
    "\n"
    "The source directory may also be provided as a mount option (e.g., in\n"
    "your fstab). When pointing to a URL, this is expected to resolve to a\n"
    "manifest file (i.e., a file of CRAM URLs).\n"
    "\n"
    "Note: The search order for CRAM reference files is:\n"
    " * Per the REF_CACHE environment variable;\n"
    " * Per the REF_PATH environment variable;\n"
    " * As specified in the CRAM's UR header tag.\n",
  me);
}

/**
  @brief   Parse and set extra-FUSE options
  @param   data     ...
  @param   arg      ...
  @param   key      ...
  @param   outargs  ...
  @return  ...
*/
static int cramp_fuse_options(void* data, const char* arg, int key, struct fuse_args* outargs) {
  cramp_fuse_conf_t *conf = data;

  switch(key) {
    case CRAMP_FUSE_CONF_KEY_HELP:
      usage(outargs->argv[0]);
      exit(1);

    case CRAMP_FUSE_CONF_KEY_VERSION:
      /* Show version information 
         n.b., fuse_version() returns an integer: (maj * 10) + min */
      (void)fprintf(stderr,
        "13 Amp %s\n"
        " * HTSLib %s\n"
        " * FUSE %s\n",
      PACKAGE_VERSION, hts_version(), str_fuse_version());
      exit(0);

    case CRAMP_FUSE_CONF_KEY_DEBUG_ME:
      conf->debug_level |= DEBUG_ME;
      /* Any debugging => foreground */
      (void)fuse_opt_add_arg(outargs, "-f");
      break;

    case CRAMP_FUSE_CONF_KEY_DEBUG_ALL:
      conf->debug_level |= DEBUG_FUSE;
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
  static cramp_fuse_t cramp_fuse;
  memset(&cramp_fuse, 0, sizeof(cramp_fuse));

  /* Initialise settings */
  static cramp_fuse_conf_t cramp_fuse_conf;
  memset(&cramp_fuse_conf, 0, sizeof(cramp_fuse_conf));
  cramp_fuse.conf = &cramp_fuse_conf;

  /* Pointer to context */
  cramp_fuse_t* ctx = &cramp_fuse;

  /* Parse command line arguments via FUSE */
  struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
  (void)fuse_opt_parse(&args, &cramp_fuse_conf, cramp_fuse_opts, cramp_fuse_options);

  /* Sanitise source directory */
  if (ctx->conf->source == NULL) {
    /* Default to current working dir */
    ctx->conf->source = xgetcwd();

  } else {
    /* Check path exists as a directory */
    struct stat s;
    int ret = lstat(ctx->conf->source, &s);

    if (ret == -1) {
      if (errno == ENOENT) {
        cramp_log_fatal(ENOENT, "\"%s\" does not exist", ctx->conf->source);
      } else {
        cramp_log_fatal(errno, "Couldn't stat \"%s\"", ctx->conf->source);
      }

    } else {
      if (!S_ISDIR(s.st_mode)) {
        cramp_log_fatal(ENOTDIR, "\"%s\" is not a directory", ctx->conf->source);
      }
    }
  }

  /* Let's go! */
  return fuse_main(args.argc, args.argv, &cramp_ops, &cramp_fuse);
}
