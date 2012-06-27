#include "test.h"
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fuse.h>
#include <libgen.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/xattr.h>
#include "log.h"

static int fserror(char *str)
{
    int ret = -errno;
    
    log_msg("    ERROR %s: %s\n", str, strerror(errno));
    
    return ret;
}
static void fsfullpath(char fpath[PATH_MAX], const char *path)
{
    strcpy(fpath, FS_DATA->rootdir);
    strncat(fpath, path, PATH_MAX);

    log_msg("    fsfullpath:  rootdir = \"%s\", path = \"%s\", fpath = \"%s\"\n",
            FS_DATA->rootdir, path, fpath);
}

int fs_utime(const char *path, struct utimbuf *ubuf)
{
    int retstat = 0;
    char fpath[PATH_MAX];

    log_msg("\nfs_utime(path=\"%s\", ubuf=0x%08x)\n",
            path, ubuf);
    fsfullpath(fpath, path);

    retstat = utime(fpath, ubuf);
    if (retstat < 0)
        retstat = fserror("fs_utime utime");

    return retstat;
}

int fsgetattr(const char *path, struct stat *statbuf)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    
    log_msg("\nfsgetattr(path=\"%s\", statbuf=0x%08x)\n",
	  path, statbuf);
    fsfullpath(fpath, path);
    
    retstat = lstat(fpath, statbuf);
    if (retstat != 0)
	retstat = fserror("fsgetattr lstat");
    
    log_stat(statbuf);
    
    return retstat;
}

int fsreadlink(const char *path, char *link, size_t size)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    
    log_msg("fsreadlink(path=\"%s\", link=\"%s\", size=%d)\n",
	  path, link, size);
    fsfullpath(fpath, path);
    
    retstat = readlink(fpath, link, size - 1);
    if (retstat < 0)
	retstat = fserror("fsreadlink readlink");
    else  {
	link[retstat] = '\0';
	retstat = 0;
    }
    
    return retstat;
}


int fsmkdir(const char *path, mode_t mode)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    
    log_msg("\nfsmkdir(path=\"%s\", mode=0%3o)\n",
	    path, mode);
    fsfullpath(fpath, path);
    
    retstat = mkdir(fpath, mode);
    if (retstat < 0)
	retstat = fserror("directory creation error");
    
    return retstat;
}

int fsunlink(const char *path)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    
    log_msg("fsunlink(path=\"%s\")\n",
	    path);
    fsfullpath(fpath, path);
    
    retstat = unlink(fpath);
    if (retstat < 0)
	retstat = fserror("fsunlink unlink");
    
    return retstat;
}

int fsrmdir(const char *path)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    
    log_msg("fsrmdir(path=\"%s\")\n",
	    path);
    fsfullpath(fpath, path);
    
    retstat = rmdir(fpath);
    if (retstat < 0)
	retstat = fserror("fsrmdir rmdir");
    
    return retstat;
}

int fssymlink(const char *path, const char *link)
{
    int retstat = 0;
    char flink[PATH_MAX];
    
    log_msg("\nfssymlink(path=\"%s\", link=\"%s\")\n",
	    path, link);
    fsfullpath(flink, link);
    
    retstat = symlink(path, flink);
    if (retstat < 0)
	retstat = fserror("fssymlink symlink");
    
    return retstat;
}

int fsopen(const char *path, struct fuse_file_info *fi)
{
    int retstat = 0;
    int fd;
    char fpath[PATH_MAX];
    
    log_msg("\nfsopen(path\"%s\", fi=0x%08x)\n",
	    path, fi);
    fsfullpath(fpath, path);
    
    fd = open(fpath, fi->flags);
    if (fd < 0)
	retstat = fserror("fsopen open");
    
    fi->fh = fd;
    log_fi(fi);
    
    return retstat;
}

int fsread(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    int retstat = 0;
    
    log_msg("\nfsread(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n",
	    path, buf, size, offset, fi);
    log_fi(fi);
    
    retstat = pread(fi->fh, buf, size, offset);
    if (retstat < 0)
	retstat = fserror("fsread read");
    
    return retstat;
}

int fswrite(const char *path, const char *buf, size_t size, off_t offset,
	     struct fuse_file_info *fi)
{
    int retstat = 0;
    
    log_msg("\nfswrite(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n",
	    path, buf, size, offset, fi
	    );
    log_fi(fi);
	
    retstat = pwrite(fi->fh, buf, size, offset);
    if (retstat < 0)
	retstat = fserror("fswrite pwrite");
    
    return retstat;
}

int fsstatfs(const char *path, struct statvfs *statv)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    
    log_msg("\nfsstatfs(path=\"%s\", statv=0x%08x)\n",
	    path, statv);
    fsfullpath(fpath, path);
    
    retstat = statvfs(fpath, statv);
    if (retstat < 0)
	retstat = fserror("fsstatfs statvfs");
    
    log_statvfs(statv);
    
    return retstat;
}


int fsopendir(const char *path, struct fuse_file_info *fi)
{
    DIR *dp;
    int retstat = 0;
    char fpath[PATH_MAX];
    
    log_msg("\nfsopendir(path=\"%s\", fi=0x%08x)\n",
	  path, fi);
    fsfullpath(fpath, path);
    
    dp = opendir(fpath);
    if (dp == NULL)
	retstat = fserror("fsopendir opendir");
    
    fi->fh = (intptr_t) dp;
    
    log_fi(fi);
    
    return retstat;
}

int fsreaddir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
	       struct fuse_file_info *fi)
{
    int retstat = 0;
    DIR *dp;
    struct dirent *de;
    
    log_msg("\nfsreaddir(path=\"%s\", buf=0x%08x, filler=0x%08x, offset=%lld, fi=0x%08x)\n",
	    path, buf, filler, offset, fi);
    dp = (DIR *) (uintptr_t) fi->fh;

    de = readdir(dp);
    if (de == 0) {
	retstat = fserror("fsreaddir readdir");
	return retstat;
    }

    do {
	log_msg("calling filler with name %s\n", de->d_name);
	if (filler(buf, de->d_name, NULL, 0) != 0) {
	    log_msg("    ERROR fsreaddir filler:  buffer full");
	    return -ENOMEM;
	}
    } while ((de = readdir(dp)) != NULL);
    
    log_fi(fi);
    
    return retstat;
}

int fsreleasedir(const char *path, struct fuse_file_info *fi)
{
    int retstat = 0;
    
    log_msg("\nfsreleasedir(path=\"%s\", fi=0x%08x)\n",
	    path, fi);
    log_fi(fi);
    
    closedir((DIR *) (uintptr_t) fi->fh);
    
    return retstat;
}

int fscreate(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    int fd;
    
    log_msg("\nfscreate(path=\"%s\", mode=0%03o, fi=0x%08x)\n",
	    path, mode, fi);
    fsfullpath(fpath, path);
    
    fd = creat(fpath, mode);
    if (fd < 0)
	retstat = fserror("fscreate creat");
    
    fi->fh = fd;
    
    log_fi(fi);
    
    return retstat;
}

//this structure defines the operation to be done on filesystem
struct fuse_operations fsoper = {
  .getattr = fsgetattr,
  .readlink = fsreadlink,
  .mkdir = fsmkdir,
  .unlink = fsunlink,
  .utime = fs_utime,
  .rmdir = fsrmdir,
  .symlink = fssymlink,
  .open = fsopen,
  .read = fsread,
  .write = fswrite,
  .statfs = fsstatfs,
  .opendir = fsopendir,
  .readdir = fsreaddir,
  .create = fscreate,
};

void usage()
{
    fprintf(stderr, "usage:  testfs rootdir mountpoint\n");
    abort();
}

int main(int argc, char *argv[])
{
    int i;
    int fuse_stat;
    struct fsstate *fsdata;

    fsdata = malloc(sizeof(struct fsstate));
    if (fsdata == NULL) {
	perror("memory error ");
	abort();
    }
    
    fsdata->logfile = log_open();
    
    for (i = 1; (i < argc) && (argv[i][0] == '-'); i++)
	if (argv[i][1] == 'o') i++;//TODO -o options  
    
    if ((argc - i) != 2) 
         usage();
    
    fsdata->rootdir = realpath(argv[i], NULL);//to get the absolute path

    argv[i] = argv[i+1];
    argc--;

    fprintf(stderr, "call to fuse_main\n");
    fuse_stat = fuse_main(argc, argv, &fsoper, fsdata);
    fprintf(stderr, "fuse_main returned %d\n", fuse_stat);
    
    return fuse_stat;
}
