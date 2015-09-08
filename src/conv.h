/* GPLv3 or later
 * Copyright (c) 2015 Genome Research Limited */

#ifndef _CRAMP_CONV_H
#define _CRAMP_CONV_H

extern off_t   cramp_conv_size(const char*);
extern ssize_t cramp_conv_read(htsFile*, char*, size_t, off_t);

#endif
