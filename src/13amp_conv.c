/* GPLv3 or later
 * Copyright (c) 2015 Genome Research Limited */

#include "config.h"

#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include "13amp.h"
#include "13amp_log.h"
#include "13amp_util.h"

#include <fuse.h>

#include <htslib/hts.h>
#include <htslib/sam.h>

/*
  NOTES

  To convert a CRAM file to a BAM file, we use HTSLib in the same way
  that `samtools view` does. That is, we open and read the CRAM file
  while writing to a newly opened BAM file; it's pretty straightforward.
  
  hts_open, however, takes a string parameter for its file reference
  (i.e., file name), accepting `-` for `stdout`. We want to spool the
  converted data directly into memory, so our "trick" (hack) is to
  create a pipe and redirect `stdout` down the write end, while another
  thread is `read`ing the read end. (We use threads, instead of `fork`,
  to reduce overhead.)

  Redirecting `stdout` using `dup2` is not thread safe! We have
  instigated a mutex to at least lock all our write calls, but obviously
  nothing else (e.g., FUSE, HTSLib, etc.) will respect that. Ultimately,
  we need to write the conversion to memory directly, rather than using
  this pipe hack. That will involve some HTSLib trickery. Then we can
  also investigate proper random access.

  On that last note, we are currently doing linear seeking from the
  start of the file. This is hopelessly inefficient, but it proves the
  concept! The difficulty of random access will be mapping the seek
  offset from the BAM to the CRAM; it will be far from linear...
*/

/**
  @brief   Argument structure to pass into conversion function
  @var     cramp    CRAM file pointer
  @var     pipe_fd  File descriptor for the write end of a pipe
*/
struct conv_args {
  htsFile* cramp;
  int      pipe_fd;
};

/**
  @brief   Argument structure to pass into the transformation function
  @var     args     Pointer to specific transformation function arguments
  @var     pipe_fd  File descriptor for the read end of a pipe
*/
struct trans_args {
  void* args;
  int   pipe_fd;
};

/**
  @brief   Argument structure to pass into the size transformation
  @var     bam_size  Size of the converted BAM file
*/
struct size_args {
  ssize_t bam_size;
};

/**
  @brief   Argument structure to pass into the read transformation
  @var     from    Offset to read from (bytes)
  @var     bytes   Maximum number of bytes to read
  @var     buffer  Pointer to data buffer
  @var     size    Actual number of bytes read (i.e., buffer size)
*/
struct read_args {
  off_t   from;
  size_t  bytes;
  char*   buffer;
  ssize_t size;
};

/**
  @brief   Convert CRAM to BAM and write data into a pipe
  @param   argv  Pointer to argument structure
  @return  Exit status (NULL = OK)
*/
void* convert(void* argv) {
  cramp_fuse_t* ctx = CTX;
  struct conv_args* args = (struct conv_args*)argv;

  /* Redirect stdout to pipe and lock mutex */
  /* FIXME Access to ctx->whatever, including the mutex, segfaults */
  /* (void)pthread_mutex_lock(&(ctx->mtx_stdout)); */
  int saved_stdout = dup(STDOUT_FILENO);
  (void)dup2(args->pipe_fd, STDOUT_FILENO);

  /* Initialise output BAM */
  bam1_t*  bam    = bam_init1();
  htsFile* output = hts_open("-", "wb");

  /* Write header */
  bam_hdr_t* header = sam_hdr_read(args->cramp);
  sam_hdr_write(output, header);

  /* Write data body */
  while (sam_read1(args->cramp, header, bam) >= 0) {
    if (sam_write1(output, header, bam) < 0) break;
  }

  hts_close(output);
  bam_destroy1(bam);
  close(args->pipe_fd);

  /* Restore stdout and unlock mutex */
  (void)dup2(saved_stdout, STDOUT_FILENO);
  (void)close(saved_stdout);
  /* FIXME Access to ctx->whatever, including the mutex, segfaults */
  /* (void)pthread_mutex_unlock(&(ctx->mtx_stdout)); */

  pthread_exit(NULL);
}

/**
  @brief   Consume data from a pipe and aggregate its size
  @param   argv  Pointer to argument structure
  @return  Exit status (NULL = OK)
*/
void* trans_size(void* argv) {
  struct trans_args* args = (struct trans_args*)argv;
  struct size_args* targs = (struct size_args*)(args->args);

  ssize_t i    = 0;
  void*   data = malloc(PIPE_BUF);

  /* Read from pipe to get size */
  while ((i = read(args->pipe_fd, data, PIPE_BUF)) > 0) {
    targs->bam_size += i;
  }

  close(args->pipe_fd);
  free(data);

  pthread_exit(NULL);
}

/**
  @brief   Consume a particular chunk of data from a pipe
  @param   argv  Pointer to argument structure
  @return  Exit status (NULL = OK)
*/
void* trans_read(void* argv) {
  struct trans_args* args = (struct trans_args*)argv;
  struct read_args* targs = (struct read_args*)(args->args);

  /* TODO Read from the pipe into a circular buffer until reaching the 
     specified offset, then read the specified amount of data into the
     output buffer while updating the buffer size (modulo EOF)        */

  targs->size = -1;
  errno = ENOSYS;

  pthread_exit(NULL);
}

/**
  @brief   Write the BAM, converted from a CRAM, down a pipe to a transformation
  @param   cramp      CRAM file pointer
  @param   transform  Transformation function
  @param   args       Arguments for the transformation function
  @return  Exit status (0 = OK; -errno = not so much)
*/
int conv_pipe(htsFile* cramp, void*(*transform)(void*), void* args) {
  /* Create the pipe */
  int pipe_fd[2];
  if (pipe(pipe_fd) == -1) {
    return -errno;
  }

  /* Set thread arguments */
  struct conv_args c_args = { cramp, pipe_fd[1] };
  struct trans_args t_args = { args, pipe_fd[0] };

  /* Create joinable conversion and transformation threads */
  pthread_t t_conv, t_trans;
  pthread_attr_t attr;

  (void)pthread_attr_init(&attr);
  (void)pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

  if (pthread_create(&t_conv, &attr, convert, (void*)&c_args)) {
    return -errno;
  }
  if (pthread_create(&t_trans, &attr, transform, (void*)&t_args)) {
    return -errno;
  }

  (void)pthread_attr_destroy(&attr);

  if (pthread_join(t_conv, NULL)) {
    return -errno;
  }
  if (pthread_join(t_trans, NULL)) {
    return -errno;
  }

  return 0;
}

/**
  @brief   Calculate the size of a BAM, converted from a CRAM
  @param   path  Path to CRAM file
  @return  Size of the converted BAM file

  TODO There is no caching of any kind, which makes this hopelessly
  inefficient. At this point, it's just to prove the concept works!

  Potential caching and precalculation mechanisms:

  * Store the file size of the conversion against some hash of the input
    CRAM, which could be serialised to disk. What that hash would be is
    a question; it ought to be cheaper than the conversion. Probably the
    MD5 sum, but that's still quite expensive given likely inputs of
    several GBs.

  * Use a background thread pool that watches the source directory for
    CRAM files; when it spots a new one (or at start up), it calculates
    the converted BAM size. (As opposed to on-demand size calculation.)

  The OS needs the size to signal the EOF to other syscalls. AFAIK,
  there is no way we can do this from userland, even though we know when
  we hit the EOF of the input CRAM.

  This may make a remote source a complete non-starter, as it would be
  far too expensive to perform this -- even with any of the above
  optimisations -- over HTTP.
*/
off_t cramp_conv_size(const char* path) {
  struct size_args filesize = { 0 };
  
  htsFile* cramp = hts_open(path, "r");
  (void)conv_pipe(cramp, trans_size, (void*)&filesize);
  (void)hts_close(cramp);

  LOG("BAM of %s is %lu bytes", path, filesize.bam_size);
  return filesize.bam_size;
}

/**
  @brief   Read the BAM file, converted from a CRAM, into the buffer
  @param   cramp   CRAM file pointer
  @param   buf     Data buffer
  @param   size    Data size (bytes)
  @param   offset  Data offset (bytes)
  @return  Exit status (Success: number of bytes read; Fail: -1)
  
  TODO Set errno like pread

  TODO This performs a linear read, with no caching, so is hopelessly
  inefficient. At this point, it's just to prove the concept works!
*/
ssize_t cramp_conv_read(htsFile* cramp, char* buf, size_t size, off_t offset) {
  struct read_args data = { offset, size, buf, 0 };
  (void)conv_pipe(cramp, trans_read, (void*)&data);
  return data.size;
}
