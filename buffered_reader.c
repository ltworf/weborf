/*
Weborf
Copyright (C) 2009-2018  Salvo "LtWorf" Tomaselli

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

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>

#include "buffered_reader.h"
#include "myio.h"

/**
This funcion inits the struct allocating a buffer of the specified size.
It will return 0 on success and 1 on fail.
*/
int buffer_init(buffered_read_t* buf, ssize_t size)
{
    buf->buffer = malloc(sizeof(char) * size);
    buf->size = size;
    buffer_reset(buf);
    return (buf->buffer == NULL) ? 1 : 0;
}

/**
Resets the pointers, so the buffer is ready for new reads on new
file descriptor
*/
void buffer_reset (buffered_read_t * buf) {
    buf->end = buf->start = buf->buffer;
}

/**
This function will free the memory allocated by the buffer used in the struct.
*/
void buffer_free(buffered_read_t * buf) {
    free(buf->buffer);
}

static ssize_t buffer_fill(fd_t fd, buffered_read_t * buf) {
    ssize_t r;

    buf->start = buf->buffer;

    //Timeout implementation
    struct pollfd monitor[1];
    monitor[0].fd = myio_getfd(fd); //File descriptor to monitor
    monitor[0].events = POLLIN; //Monitor on input events

    //Waits the timeout or reads the data.
    //If timeout is reached and no input is available
    //will behave like the stream is closed.
    if (poll(monitor, 1, READ_TIMEOUT) == 0) {
        r = 0;
    } else {
        r = myio_read(fd, buf->buffer, buf->size);
    }

    if (r <= 0) { //End of the stream
        buf->end = buf->start;
    } else {
        buf->end = buf->start + r;
    }

    return r;
}

/**
This function is designed to be similar to a normal read, but it uses an internal
buffer.
When the buffer is empty, it will try to fill it.
An important difference from the normal read is that this function will wait until
the requested amount of bytes are available, or until the timeout occurs.
Timeout duration is defined with the READ_TIMEOUT define.
On some special cases, the read data could be less than the requested one. For example if
end of file is reached and it is impossible to do further reads.
*/
ssize_t buffer_read(fd_t fd, void *b, ssize_t count, buffered_read_t * buf) {
    ssize_t wrote = 0;              //Count of written bytes
    ssize_t available, needed;      //Available bytes in buffer, and requested bytes remaining


    while (wrote < count) {
        available = buf->end - buf->start;
        needed = count - wrote;

        if (needed <= available) { //More data in buffer than needed
            memcpy(b, buf->start, needed);
            buf->start += needed;
            return wrote + needed;
        } else { //Requesting more data than available
            if (available > 0) {//Empty the remaining data in the buffer
                memcpy(b, buf->start, available);
                b += available;
                wrote += available;
            }
            //Filing the buffer again
            if (buffer_fill(fd,buf) <= 0) { //End of the stream
                return wrote;
            }
        }

    } /*while */
    return wrote;
}


/**
 * This function returns how many bytes must be read in order to
 * read enough data for it to end with the string needle.
 * strlen(needle) must be added to the returned value to include the
 * needle string in the result of the read.
 *
 * If the string needle is not in the buffer then the entire size
 * of the data present in cache will be return.
 * */
size_t buffer_strstr(fd_t fd, buffered_read_t * buf, char * needle) {
    if (buf->end - buf->start==0) {
        buffer_fill(fd,buf);
    }

    buf->end[0]=0;
    char *r=strstr(buf->start,needle);

    if (r==NULL) {
        return buf->end - buf->start;
    } else {
        return r-buf->start;

    }

}
