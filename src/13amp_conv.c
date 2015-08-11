/* GPLv3 or later
 * Copyright (c) 2015 Genome Research Limited */

#include "config.h"

#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>

#include "13amp_log.h"

#include <htslib/hts.h>

/**
  @brief   Read the BAM file, converted from a CRAM, into the buffer
  @param   cramp   CRAM file pointer
  @param   buf     Data buffer
  @param   size    Data size (bytes)
  @param   offset  Data offset (bytes)
  @return  Exit status (Success: number of bytes read; Fail: -1)
  
  Errors:
    [TODO, also per read(2)]
*/
int cram2bam(htsFile* cramp, char* buf, size_t size, off_t offset) {
  /* TODO */
  LOG("TODO: CRAM-to-BAM conversion");
  errno = ENOSYS;
  return -1;
}
