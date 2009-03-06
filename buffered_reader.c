/*
Weborf
Copyright (C) 2009  Salvo "LtWorf" Tomaselli

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

#include "buffered_reader.h"
#include <unistd.h>
#include <stdlib.h>

/**
This funcion inits the struct allocating a buffer of the specified size.
It will return 0 on success and 1 on fail.
*/
int buffer_init(buffered_read_t * buf, int size) {
    buf->buffer = malloc(sizeof(char*) * size);
    buf->start= buf->buffer;
    buf->end=buf->buffer;
    
    return (buf->buffer == NULL) ? 1 : 0;
}

/**
This function will free the memory allocated by the buffer used in the struct.
*/
void buffer_free(buffered_read_t * buf) {
    if (buf->buffer != NULL)
        free(buf->buffer);
}

ssize_t buffer_read(int fd, void *b, size_t count,buffered_read_t  buf) {
    
}
