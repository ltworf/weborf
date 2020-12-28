/*
Weborf
Copyright (C) 2010-2020  Salvo "LtWorf" Tomaselli

Weborf is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

@author Salvo "LtWorf" Tomaselli <tiposchi@tiscali.it>
*/
#include "options.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <syslog.h>
#include <stdbool.h>

#include "cachedir.h"
#include "utils.h"
#include "types.h"
#include "instance.h"


char *cachedir=NULL;

/**
Generates the filename for the cached entity and stores it in the buffer
*/
static inline void cached_filename(unsigned int uprefix,connection_t *connection_prop, char *buffer) {
    snprintf(buffer,PATH_LEN,"%s/%u-%llu-%llu-%ld",cachedir,uprefix,(unsigned long long int)connection_prop->strfile_stat.st_ino,(unsigned long long int)connection_prop->strfile_stat.st_dev,connection_prop->strfile_stat.st_mtime);
}

/**
Returns true if the caching is enabled and
false otherwise
*/
bool cache_is_enabled() {
    return cachedir != NULL;
}

/**
Sends a cached item if present and returns true.
Returns false on cache miss.
*/
bool cache_send_item(unsigned int uprefix,connection_t* connection_prop) {//Try to send the cached file instead
    int cachedfd=cache_get_item_fd(uprefix,connection_prop);

    if (cachedfd==-1)
        return false;

    int oldfd=connection_prop->strfile_fd;
    connection_prop->strfile_fd=cachedfd;

    /*
    replaces the stat of the directory with the stat of the cached file
    it is safe here since get_cached_item has already been executed
    */
    fstat(connection_prop->strfile_fd, &connection_prop->strfile_stat);

    write_file(connection_prop);

    //Restore file descriptor so it can be closed later
    connection_prop->strfile_fd=oldfd;

    //Closes the cache file descriptor
    close(cachedfd);

    return true;
}

/**

This function returns the file descriptor to the file containing the cached
item.

If a cache miss occurs -1 will be returned
If cache is not in use (cache_init was not called) it will always return -1

uprefix is an integer that must be unique for each call of get_cached_dir.
Its purpose is to distinguish between calls that will eventually generate an
HTML file and calls that will generate XML or other data.
So the directory would be the same but the generated content is different.

Acquires a shared lock on the file, so if the file is already opened in write mode
the lock will fail and the function will return the same result of a cache miss.
*/
int cache_get_item_fd(unsigned int uprefix,connection_t* connection_prop) {
    if (!cachedir) return -1;

    char fname[PATH_LEN];

    //Get the filename
    cached_filename(uprefix,connection_prop,fname);

    int fd=open(fname,O_RDONLY);
    if (fd==-1) return -1; //Cache miss

    //Acquire lock on the file and return the file descriptor
    if (flock(fd,LOCK_SH|LOCK_NB)==0)
        return fd;

    //Lock could not be acquired
    close(fd);
    return -1;
}


/**
Same as cache_get_item_fd but here the file is created and opened for
reading and writing, and will be created if it doesn't exist

An exclusive lock will be acquired over the file, since it is being opened
for writing too.
*/
int cache_get_item_fd_wr(unsigned int uprefix,connection_t *connection_prop) {
    if (!cachedir) return -1;

    char fname[PATH_LEN];

    //Get the filename
    cached_filename(uprefix,connection_prop,fname);

    int fd = open(fname,O_RDWR| O_CREAT,S_IRUSR|S_IWUSR);

    //Acquire the exclusive lock in a non-blocking way
    if (flock(fd,LOCK_EX|LOCK_NB)==0) {
        return fd;
    }
    close(fd);
    return -1;

}

/**
Stores the content of the buffer "content" in cache, for the size specified by content_len
The cache file will have an exclusive lock to prevent multiple threads accessing the same file
in write mode.

Lock is acquired in a non-blocking way, so if the file is already locked the method will just return
without writing anything on the file. This behavior is wanted because it is assumed that the lock is held
by another thread writing the same content on the file, waiting for the lock to be released would lead to
override the content of the file with the same content.
*/
void cache_store_item(unsigned int uprefix,connection_t* connection_prop, char *content, size_t content_len) {
    if (!cachedir) return;

    char fname[PATH_LEN];
    cached_filename(uprefix,connection_prop,fname);

    int fd=open(fname, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);

    if (fd == -1) return; //Just do nothing in case of error

    //Try to acquire lock in a non-blocking way, and exit if another thread has that lock
    if (flock(fd,LOCK_EX|LOCK_NB)!=0) {
        close(fd);
        return;
    }

    if (write(fd, content, content_len) != content_len)
        unlink(fname);
    close(fd);
    return;
}



/**
This function initializes a cache directory.
If the dir is not a directory or it is impossible to stat
weborf will terminate.
If it is impossible to delete and create files in it, weborf
will just log a warning.
*/
void cache_init(char* dir) {
    cachedir=dir;

    {
        //Check if it exists
        struct stat stat_buf;
        if (stat(dir, &stat_buf)!=0) {
            dprintf(2, "Unable to stat cache directory\n");
#ifdef SERVERDBG
            syslog(LOG_ERR,"Unable to stat cache directory");
#endif
            exit(10);
        }

        //Check it is a directory
        if (!S_ISDIR(stat_buf.st_mode)) {
            dprintf(2, "--cache parameter must be a directory\n");
#ifdef SERVERDBG
            syslog(LOG_ERR,"--cache parameter must be a directory");
#endif
            exit(10);
        }
    }

    //check we have permissions
    if (access(dir,W_OK|R_OK)!=0) {
        dprintf(2,"no read or write permissions on cache dir\n");
#ifdef SERVERDBG
        syslog(LOG_ERR,"no read or write permissions on cache dir");
#endif
        exit(10);
    }
}
