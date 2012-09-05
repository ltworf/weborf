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
#include "options.h"

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>

#include "buffered_reader.h"
#include "types.h"

/**
This funcion inits the struct allocating a buffer of the specified size.
It will return 0 on success and 1 on fail.
*/
int buffer_init(buffered_read_t * buf, size_t size) {
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

/**
 * Fills the buffer.
 *
 * RETURN VALUE: how many bytes were copied into the buffer
 * If it returns -1 it means that the buffer was not empty.
 *
 * 0 means connection closed by the peer (or timeout)
 **/
ssize_t buffer_fill(int fd, buffered_read_t * buf) {
    ssize_t r;

    if (buf->end - buf->start>0) {
        return -1;
    }

    buf->start = buf->buffer;

    //Timeout implementation
    struct pollfd monitor[1];
    monitor[0].fd = fd; //File descriptor to monitor
    monitor[0].events = POLLIN; //Monitor on input events

    //Waits the timeout or reads the data.
    //If timeout is reached and no input is available
    //will behave like the stream is closed.
    if (poll(monitor, 1, READ_TIMEOUT) == 0) {
        r = 0;
    } else {
        r = read(fd, buf->buffer, buf->size);
    }

    if (r <= 0) { //End of the stream
        buf->end = buf->start;
    } else {
        buf->end = buf->start + r;
    }

    return r;
}

/**
 * Same as buffer_read but does not trigger a buffer_fill if
 * there is not enough data to return.
 **/
size_t buffer_read_non_fill(int fd, void *b, size_t count, buffered_read_t * buf) {
    size_t available= buf->end - buf->start;

    if (available==0) {
        return 0;
    } else if (count > available) { //More data in buffer than needed
        count=available;
    }

    memcpy(b, buf->start, count);
    buf->start += count;
    return count;
}

/**
This function is designed to be similar to a normal read, but it uses an internal
buffer.
When the buffer is empty, it will try to fill it.
An important difference from the normal read is that this function will wait until
the requested amount of bytes is available, or until the timeout occurs.
Timeout duration is defined with the READ_TIMEOUT define.
On some special cases, the read data could be less than the requested one. For example if
end of file is reached and it is impossible to do further reads.
*/
size_t buffer_read(int fd, void *b, size_t count, buffered_read_t * buf) {
    size_t wrote = 0;              //Count of written bytes
    size_t available, needed;      //Available bytes in buffer, and requested bytes remaining
#warning "Do not use buffer_read"

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
 * This function flushes all the buffered data to a file descriptor.
 * It is possible to limit the amount of data to be copied to the file descriptor.
 * This function is useful when switching from buffered reads to normal ones.
 *
 * dest: destination file descriptor
 * buf: pointer to the buffered_read_t data structure
 * limit: maximum amount of bytes to flush to the file descriptor. To avoid limits
 *     it is sufficient to pass a MAXINT.
 *
 * Returns the amount of written data, which might be different than the amount
 * of data previously present on the buffer.
 */
ssize_t buffer_flush_fd(int dest, buffered_read_t * buf,size_t limit) {
    size_t available= buf->end - buf->start;

    size_t dim = limit<available?limit:available;

    ssize_t count=write(dest,buf->start,dim);
    buf->start +=dim;

    return count;
}


/**
 * This function returns how many bytes must be read in order to
 * read enough data for it to end with the string needle.
 * strlen(needle) must be added to the returned value to include the
 * needle string in the result of the read.
 *
 * If the string needle is not in the buffer then the entire size
 * of the data present in cache will be return.
 */
size_t buffer_strstr(int fd, buffered_read_t * buf, char * needle) {
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


/*#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

int main () {
    buffered_read_t buf;

    buffer_init(&buf, 30);

    int fp=open("/home/salvo/.bash_history",O_RDONLY);
    char * k=malloc (600);

    int end=0;

    while (1) {

        int size=buffer_strstr(fp,&buf,"\n")+1;
        end=buffer_read(fp,k,size,&buf);
        if (end<size || end==0) break;

        k[end]=0;
        printf("%d ---- %s\n",size,k);
    }
    printf("%d",end);

    free(k);
    buffer_free(&buf);
}
*/
