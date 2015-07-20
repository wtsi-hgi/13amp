/* GPLv3 or later
 * Copyright (c) 2015 Genome Research Limited */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "13amp.h"
#include "13amp_ops.h"

#include <fuse.h>
#include <fuse_opt.h>


/* FUSE Operations */
static struct fuse_operations cramp_ops = {
  /* .init    = cramp_init, */
  .getattr = cramp_getattr,
  .open    = cramp_open,
  .read    = cramp_read,
  .readdir = cramp_readdir,
  .release = cramp_release,
  /* .destroy = cramp_destroy */
};

/* FUSE Options */
static struct fuse_opt cramp_fuse_opts[] = {
  /* TODO */
  CRAMP_FUSE_OPT("-s %s",       source, 0),
  CRAMP_FUSE_OPT("--source %s", source, 0),
  CRAMP_FUSE_OPT("source=%s",   source, 0),

  CRAMP_FUSE_OPT("-r %s",       ref,    0),
  CRAMP_FUSE_OPT("--ref %s",    ref,    0),
  CRAMP_FUSE_OPT("ref=%s",      ref,    0),
  
  FUSE_OPT_KEY("--debug",        CRAMP_FUSE_CONF_KEY_DEBUG_ME), /* the --debug option is only recongnised by iquestFuse, not FUSE itself */
  FUSE_OPT_KEY("--debug-trace",  CRAMP_FUSE_CONF_KEY_TRACE_ME), 

  FUSE_OPT_KEY("-V",             CRAMP_FUSE_CONF_KEY_VERSION),
  FUSE_OPT_KEY("--version",      CRAMP_FUSE_CONF_KEY_VERSION),

  FUSE_OPT_KEY("-h",             CRAMP_FUSE_CONF_KEY_HELP),
  FUSE_OPT_KEY("--help",         CRAMP_FUSE_CONF_KEY_HELP),

  FUSE_OPT_KEY("-d",             CRAMP_FUSE_CONF_KEY_DEBUG_ALL),
  FUSE_OPT_KEY("debug",          CRAMP_FUSE_CONF_KEY_DEBUG_ALL),

  FUSE_OPT_KEY("-f",             CRAMP_FUSE_CONF_KEY_FOREGROUND),

  FUSE_OPT_KEY("-s",             CRAMP_FUSE_CONF_KEY_SINGLETHREAD),

  FUSE_OPT_END
};

/**
  @brief  Usage instructions to stderr
  @param  me  Program name (i.e., argv[0])
*/
void usage(char* me) {
  (void)fprintf(stderr,
    "Usage: %s mountpoint [options]\n"
    "\n"
    "Options:\n"
    "  -s | --source DIR|URI  Source directory (default .)\n"
    "  -r | --ref FILE        FASTA reference file\n"
    "  -V | --version         Print version\n"
    "  -h | --help            This helpful text\n"
    "\n"
    "Note: The search order for the reference file is:\n"
    "* As specified within the -T options;\n"
    "* Via the REF_CACHE environment variable;\n"
    "* Via the REF_PATH environment variable;\n"
    "* As specified in the UR header tag.\n"
    "\n",
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
int cramp_options(void* data, const char* arg, int key, struct fuse_args* outargs) {
  (void)fprintf(stderr, "13amp: arg=%s key=%d\n", arg, key);
  cramp_fuse_conf_t *conf = data;

  switch(key) {
  case CRAMP_FUSE_CONF_KEY_HELP:
    usage(outargs->argv[0]);
        fuse_opt_add_arg(outargs, "-ho"); /* ask fuse to print help without header */
    fuse_main(outargs->argc, outargs->argv, &cramp_ops, NULL);
    exit(1);
  case CRAMP_FUSE_CONF_KEY_VERSION:
    fprintf(stderr, "13amp version %s\n", PACKAGE_VERSION);
    fuse_opt_add_arg(outargs, "--version"); /* pass this along to FUSE for their version */
    fuse_main(outargs->argc, outargs->argv, &cramp_ops, NULL);
    exit(0);
  case CRAMP_FUSE_CONF_KEY_DEBUG_ME:
    conf->debug_level = LOG_DEBUG;
    fuse_opt_add_arg(outargs, "-f"); /* ask fuse to stay in foreground */
    break;
  case CRAMP_FUSE_CONF_KEY_TRACE_ME:
    conf->debug_level = LOG_DEBUG1;
    fuse_opt_add_arg(outargs, "-f"); /* ask fuse to stay in foreground */
    break;
  case CRAMP_FUSE_CONF_KEY_DEBUG_ALL:
    conf->debug_level = LOG_DEBUG;
    /* fall through as DEBUG implies FOREGROUND */
  case CRAMP_FUSE_CONF_KEY_FOREGROUND:
    /* could set foreground operation flag if we care */
    return 1; /* -d and -f need to be processed by FUSE */
  case CRAMP_FUSE_CONF_KEY_SINGLETHREAD:
    //TODO disable pthreads here???
    return 1; /* -s needs to be processed by FUSE */
 default:
   return 1; /* anything not recognised should be processed by FUSE */
  }
  /* anything that breaks out of the switch will NOT be processed by FUSE */
  return 0;
}

int main(int argc, char** argv) {
  static cramp_fuse_t cramp_fuse;
  memset(&cramp_fuse, 0, sizeof(cramp_fuse));

  static cramp_fuse_conf_t cramp_fuse_conf;
  memset(&cramp_fuse_conf, 0, sizeof(cramp_fuse_conf));
  cramp_fuse.conf = &cramp_fuse_conf;

  cramp_fuse_t* cf = &cramp_fuse;

  struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

  /* Defaults */
  if (cf->conf->source == NULL) {
    int bufsize = strlen(CRAMP_DEFAULT_SOURCE)+1;
    cf->conf->source = malloc(bufsize);
    if(cf->conf->source == NULL) {
      fprintf(stderr, "main: malloc(%d) setting default for cf->conf->source\n", bufsize);
      exit(2);
    }
    strncpy(cf->conf->source, CRAMP_DEFAULT_SOURCE, bufsize);
  }

  fuse_opt_parse(&args, &cramp_fuse_conf, cramp_fuse_opts, cramp_options);

  /* TODO Debug logging */
  (void)fprintf(stderr, "source: %s\n", cf->conf->source);
  (void)fprintf(stderr, "ref: %s\n", cf->conf->ref);
  return fuse_main(args.argc, args.argv, &cramp_ops, &cramp_fuse);
}
