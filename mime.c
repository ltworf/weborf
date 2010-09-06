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


#include <magic.h>
#include "mime.h"

/**
Returns a token for the libmagic.
buffer must NOT be shared amongst threads.
The size of buffer is assumed to be at least MIMETYPELEN
*/
inline int init_mime(magic_t *token) {
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
inline void release_mime(magic_t token) {
    if (token==NULL) return;
#ifdef SEND_MIMETYPES
    magic_close(token);
#endif
}

/**
returns mimetype of an opened file descriptor
*/
inline const char* get_mime_fd (magic_t token,int fd) {

#ifdef SEND_MIMETYPES
    if (token==NULL) return NULL;
    /*Dup file to work around the magic_descriptor
    that will close the file
    */
    int new_fd=dup(fd);
    return magic_descriptor(token,new_fd);
#else
    return NULL;
#endif
}

/*int main () {

    char q[500];
    magic_t cookie= magic_open(MAGIC_SYMLINK | MAGIC_MIME_TYPE);
    magic_buffer(cookie,&q, 500);
    int load=magic_load(cookie,NULL);

    char p[500];
    magic_t cookie1= magic_open(MAGIC_SYMLINK | MAGIC_MIME_TYPE);
    load=magic_load(cookie1,NULL);

    const char *f=magic_file(cookie,"/tmp/ciao");
    const char *g=magic_file(cookie1,"/boot/vmlinuz-2.6.33.3-aracne");

    //printf("%s\n",f);

    printf("%s\n",f);


    printf("%s\n",g);

    magic_close(cookie);

}*/