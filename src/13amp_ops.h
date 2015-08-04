/* GPLv3 or later
 * Copyright (c) 2015 Genome Research Limited */

#ifndef CRAMP_OPS_H
#define CRAMP_OPS_H

/* File system operations */
extern void* cramp_init(struct fuse_conn_info*);
extern int   cramp_getattr(const char*, struct stat*);
extern int   cramp_readlink(const char*, char*, size_t);
extern int   cramp_open(const char*, struct fuse_file_info*);
extern int   cramp_read(const char*, char*, size_t, off_t, struct fuse_file_info*);
extern int   cramp_release(const char*, struct fuse_file_info*);
extern int   cramp_opendir(const char*, struct fuse_file_info*);
extern int   cramp_readdir(const char*, void*, fuse_fill_dir_t, off_t, struct fuse_file_info*);
extern int   cramp_releasedir(const char*, struct fuse_file_info*);
extern void  cramp_destroy(void*);

#endif
