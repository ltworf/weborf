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
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>


#include "cachedir.h"
#include "options.h"
#include "utils.h"


char *cachedir=NULL;

/**
Generates the filename for the cached entity and stores it in the buffer
*/
static inline void cached_filename(unsigned int uprefix,connection_t *connection_prop, char *buffer) {
    snprintf(buffer,PATH_LEN,"%s/%u-%llu-%llu-%ld",cachedir,uprefix,(unsigned long long int)connection_prop->strfile_stat.st_ino,(unsigned long long int)connection_prop->strfile_stat.st_dev,connection_prop->strfile_stat.st_mtime);
}

/**

This function returns the file descriptor to the file containing the cached
directory.

If a cache miss occurs -1 will be returned
If cache is not in use (cache_init was not called) it will always return -1

uprefix is an integer that must be unique for each call of get_cached_dir.
It's purpose is to distinguish between calls that will eventually generate an
HTML file and calls that will generate XML or other data.
So the directory would be the same but the generated content is different.
*/
int get_cached_item(unsigned int uprefix,connection_t* connection_prop) {
    if (!cachedir) return -1;

    char fname[PATH_LEN];

    //Get the filename
    cached_filename(uprefix,connection_prop,fname);

    return open(fname,O_RDONLY);
}



void store_cache_item(unsigned int uprefix,connection_t* connection_prop, char *content, size_t content_len) {
    if (!cachedir) return;

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
void init_cache(char* dir) {
    cachedir=dir;


    //Check if it exists and is a directory
    {
        struct stat stat_buf;
        if (stat(dir, &stat_buf)!=0) {
            write(2,"Unable to stat cache directory\n",31);
            exit(10);
        }

        if (!S_ISDIR(stat_buf.st_mode)) {
            //Not a directory
            write(2,"--cache parameter must be a directory\n",38);
            exit(10);
        }
    }
}

/**
Removes all the files contained in the cache directory.
The cache must have been already initialized for this to work

Returns 0 on success, -1 otherwise
*/
int clear_cache() {
    if (!cachedir) return -1;

    //Empty directory
    DIR *dp = opendir(cachedir); //Open dir
    struct dirent entry;
    struct dirent *result;
    int return_code;
    int retval=0;

    if (dp == NULL) {
        return 1;
    }

    char*file=malloc(PATH_LEN);//Buffer for path
    if (file==NULL)
        return -1;

    //Cycles trough dir's elements
    for (return_code=readdir_r(dp,&entry,&result); result!=NULL && return_code==0; return_code=readdir_r(dp,&entry,&result)) { //Cycles trough dir's elements

        //skips dir . and .. but not all hidden files
        if (entry.d_name[0]=='.' && (entry.d_name[1]==0 || (entry.d_name[1]=='.' && entry.d_name[2]==0)))
            continue;

        snprintf(file,PATH_LEN,"%s/%s",cachedir, entry.d_name);
        if (unlink(file)!=0) retval=-1;
    }

    closedir(dp);
    free(file);
    return retval;

}

