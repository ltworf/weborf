/*
Weborf
Copyright (C) 2010  Salvo "LtWorf" Tomaselli

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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "cachedir.h"
#include "options.h"



char *cachedir="/tmp/weborfcache"; //TODO this is just TEMPORARY

/**
Generates the filename for the cached entity and stores it in the buffer
*/
static inline void cached_filename(int uprefix,connection_t *connection_prop, char *buffer) {
    snprintf(buffer,PATH_LEN,"%s/%d-%llu-%llu-%ld",cachedir,uprefix,connection_prop->strfile_stat.st_ino,connection_prop->strfile_stat.st_dev,connection_prop->strfile_stat.st_mtime);
}

/**
This function returns the file descriptor to the file containing the cached
directory.

If a cache miss occurs -1 will be returned

uprefix is an integer that must be unique for each call of get_cached_dir.
It's purpose is to distinguish between calls that will eventually generate an
HTML file and calls that will generate XML or other data.
So the directory would be the same but the generated content is different.
*/
int get_cached_item(int uprefix,connection_t* connection_prop) {
    char fname[PATH_LEN];

    //Get the filename
    cached_filename(uprefix,connection_prop,fname);

    return open(fname,O_RDONLY);
}


void store_cache_item(int uprefix,connection_t* connection_prop, char *content, size_t content_len) {
    char fname[PATH_LEN];
    cached_filename(uprefix,connection_prop,fname);

    int fd=open(fname,O_WRONLY| O_CREAT,S_IRUSR|S_IWUSR);

    if (fd==-1) return; //Just do nothing in case of error

    write(fd,content,content_len);
    close(fd);
    return;
}



/**
This function initializes a cache directory and makes sure it is empty.
*/
void init_cache() {

}

