/* GPLv3 or later
 * Copyright (c) 2015 Genome Research Limited */

#ifndef CRAMP_OPS_H
#define CRAMP_OPS_H

/* FUSE operations */
void* cramp_init(struct fuse_conn_info*);
int   cramp_getattr(const char*, struct stat*);
int   cramp_open(const char*, struct fuse_file_info*);
int   cramp_read(const char*, char*, size_t, off_t, struct fuse_file_info*);
int   cramp_readdir(const char*, void*, fuse_fill_dir_t, off_t, struct fuse_file_info*);
int   cramp_release(const char*, struct fuse_file_info*);
void  cramp_destroy(void*);

#endif
