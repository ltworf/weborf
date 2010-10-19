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


#include <string.h>
#include <unistd.h>


#include "mime.h"
#include "options.h"

#ifdef SEND_MIMETYPES
#include <magic.h>
#endif

/**
Returns a token for the libmagic.
buffer must NOT be shared amongst threads.
The size of buffer is assumed to be at least MIMETYPELEN
*/
int init_mime(magic_t *token) {
#ifdef SEND_MIMETYPES
    *token=magic_open(MAGIC_SYMLINK | MAGIC_MIME_TYPE);
    if (*token==NULL) return 1;
    return magic_load(*token,NULL);                     //Load the database
#else
    *token=NULL;
    return 0;
#endif

}

/**
Releases the token for libmagic
If the token is null, it will do nothing.
*/
void release_mime(magic_t token) {
    if (token==NULL) return;
#ifdef SEND_MIMETYPES
    magic_close(token);
#endif
}

/**
returns mimetype of an opened file descriptor
the cursor can be located at any position

fd is the descriptor to an open file
sb is the stat to the same file
*/
const char* get_mime_fd (magic_t token,int fd,struct stat *sb) {

#ifdef SEND_MIMETYPES
    if (token==NULL) return NULL;
    
    /*If fd is a directory, send the mimetype without attempting to read it*/
    if (sb->st_mode & S_IFDIR)
        return "application/x-directory";
    
    /*
     * Seek file to 0 and read it's header to know it's mime type
     * then seek again to the previous position
    */
    char buf[64];

    //Get the current cursor position
    unsigned long long int prev_pos=lseek(fd,0,SEEK_CUR);

    //Set the cursor to the beginning of the file
    lseek(fd,0,SEEK_SET);

    //Reads 64 bytes to determine the type
    int r=read(fd,&buf,64);

    //Reset the position
    lseek(fd,prev_pos,SEEK_SET);

    const char* mime=magic_buffer(token,&buf,r);

    return mime;

#else
    return NULL;
#endif
}