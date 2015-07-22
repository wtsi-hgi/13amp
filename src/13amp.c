/* GPLv3 or later
 * Copyright (c) 2015 Genome Research Limited */

#include "config.h"

#include <stdlib.h>

/* gnulib suggested includes */
#include <errno.h>
#include <getopt.h>
#include <locale.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "error.h"
#include "gl_avltreehash_list.h"
#include "gl_xlist.h"
#include "hash-pjw.h"
#include "progname.h"
#include "size_max.h"
#include "version-etc.h"
#include "xalloc.h"
#include "xstrndup.h"

#include "13amp.h"
#include "13amp_ops.h"
#include "13amp_log.h"

#include <fuse.h>
#include <fuse_opt.h>
#include <fuse_common.h>

#include <htslib/hts.h>

/* FUSE Operations */
static struct fuse_operations cramp_ops = {
  /* .init    = cramp_init, */
  .getattr = cramp_getattr,
  .open    = cramp_open,
  .read    = cramp_read,
  .release = cramp_release,
  /* .opendir = cramp_opendir, */
  .readdir = cramp_readdir,
  /* .releasedir = cramp_releasedir, */
  /* .destroy = cramp_destroy */
};

/* FUSE Options */
static struct fuse_opt cramp_fuse_opts[] = {
  CRAMP_FUSE_OPT("-S %s",        source, 0),
  CRAMP_FUSE_OPT("--source=%s",  source, 0),
  CRAMP_FUSE_OPT("source=%s",    source, 0),

  FUSE_OPT_KEY("--debug",        CRAMP_FUSE_CONF_KEY_DEBUG_ME),

  FUSE_OPT_KEY("-d",             CRAMP_FUSE_CONF_KEY_DEBUG_ALL),
  FUSE_OPT_KEY("debug",          CRAMP_FUSE_CONF_KEY_DEBUG_ALL),

  FUSE_OPT_KEY("-V",             CRAMP_FUSE_CONF_KEY_VERSION),
  FUSE_OPT_KEY("--version",      CRAMP_FUSE_CONF_KEY_VERSION),

  FUSE_OPT_KEY("-h",             CRAMP_FUSE_CONF_KEY_HELP),
  FUSE_OPT_KEY("--help",         CRAMP_FUSE_CONF_KEY_HELP),

  FUSE_OPT_KEY("-f",             CRAMP_FUSE_CONF_KEY_FOREGROUND),

  FUSE_OPT_KEY("-s",             CRAMP_FUSE_CONF_KEY_SINGLETHREAD),

  FUSE_OPT_END
};

/**
  @brief  Usage instructions to stderr
  @param  me  Program name (i.e., argv[0])
*/
void usage(char* me) {
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
int cramp_fuse_options(void* data, const char* arg, int key, struct fuse_args* outargs) {
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
        " * FUSE %d\n",
      PACKAGE_VERSION, hts_version(), fuse_version());
      exit(0);

    case CRAMP_FUSE_CONF_KEY_DEBUG_ALL:
      conf->debug_level |= DEBUG_FUSE;
      /* Fall through: debug all => debug me */

    case CRAMP_FUSE_CONF_KEY_DEBUG_ME:
      conf->debug_level |= DEBUG_ME;
      /* Fall through: debug me => foreground */

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

  /* Defaults */
  /* TODO Tidy and fix */
  if (ctx->conf->source == NULL) {
    int bufsize = strlen(CRAMP_DEFAULT_SOURCE)+1;
    ctx->conf->source = malloc(bufsize);
    if(ctx->conf->source == NULL) {
      (void)fprintf(stderr, "main: malloc(%d) setting default for ctx->conf->source\n", bufsize);
      exit(2);
    }
    strncpy(ctx->conf->source, CRAMP_DEFAULT_SOURCE, bufsize);
  }

  /* Log configuration */
  (void)cramp_log(ctx, "Source directory: %s",
                       ctx->conf->source);

  (void)cramp_log(ctx, "Debug level: %d",
                       ctx->conf->debug_level);

  (void)cramp_log(ctx, "Single threaded: %s",
                       ctx->conf->one_thread ? "yes" : "no");

  return fuse_main(args.argc, args.argv, &cramp_ops, &cramp_fuse);
}
