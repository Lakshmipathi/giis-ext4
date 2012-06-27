
#ifndef _PARAMS_H_
#define _PARAMS_H_

#define FUSE_USE_VERSION 26

#define _XOPEN_SOURCE 500

#include <limits.h>
#include <stdio.h>
struct fsstate {
    FILE *logfile;
    char *rootdir;
};
#define FS_DATA ((struct fsstate *) fuse_get_context()->private_data)

#endif
