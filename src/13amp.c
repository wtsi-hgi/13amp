/* GPLv3 or later
 * Copyright (c) 2015 Genome Research Limited */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include <fuse.h>
#include <fuse_opt.h>

#include "13amp_ops.h"

/* FUSE Operations */
static struct fuse_operations cramp_ops = {
  .init    = cramp_init,
  .getattr = cramp_getattr,
  .open    = cramp_open,
  .read    = cramp_read,
  .readdir = cramp_readdir,
  .release = cramp_release,
  .destroy = cramp_destroy
};

/* FUSE Options */
static struct fuse_opt fuse_options[] = {
  /* TODO */
  FUSE_OPT_END
};

/**
  @brief  Usage instructions to stderr
  @param  me  Program name (i.e., argv[0])
*/
void usage(char* me) {
  (void)fprintf(stderr,
    "Usage: %s mountpoint sourcedir [options]\n"
    "\n"
    "Options:\n"
    "  -T FILE             FASTA reference file\n"
    "  -V       --version  Print version\n"
    "  -h       --help     This helpful text\n"
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
  /* TODO */
}

int main(int argc, char **argv) {
  return 0;
}
