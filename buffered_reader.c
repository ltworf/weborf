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
#include <string.h>

/**
This funcion inits the struct allocating a buffer of the specified size.
It will return 0 on success and 1 on fail.
*/
int buffer_init(buffered_read_t * buf, int size) {
    buf->buffer = malloc(sizeof(char*) * size);
    buf->start= buf->buffer;
    buf->end=buf->buffer;
    buf->size=size;
    
    return (buf->buffer == NULL) ? 1 : 0;
}

/**
This function will free the memory allocated by the buffer used in the struct.
*/
void buffer_free(buffered_read_t * buf) {
    if (buf->buffer != NULL)
        free(buf->buffer);
}

/**
This function is designed to be similar to a normal read, but it uses an internal
buffer.
When the buffer is empty, it will try to fill it.
An important difference from the normal read is that this function will wait until
the requested amount of bytes are available.
On some special cases, the read data could be minor of the requested one. For example if
end of file is reached and it is impossible to do further reads.
*/
ssize_t buffer_read(int fd, void *b, ssize_t count,buffered_read_t * buf) {
    ssize_t wrote=0;
    //size_t _count=count; //Not necessarily needed
    ssize_t available;

    //printf("Requested %d bytes\n", count);
    while (wrote<count) {
        
        if ((available=buf->end - buf-> start) != 0 ) {//Data available in buffer
            if (count <= available) {//More data in buffer than needed
                //printf("1 Needed %d bytes, Available %d\n", count,available);
                memcpy(b, buf->start, count );
                
                /*int i;
                for (i=0; i<count;i++) {
                    printf("char: %c\n",(char)*(buf->start+i));
                }*/
                
                buf->start+=count;
                //printf("1 Needed %d bytes, Available %d\n", count,available);
                return count;
            } else {//Requesting more data than available
              //  printf("2 Needed %d bytes, Available %d\n", count,available);
                memcpy(b, buf->start, available );
                b+=available;
                buf->start+=available;
                wrote+=available;
            //    printf("2 Needed %d bytes, Available %d\n", count,available);
            }
        
        } else {//Need to read some data
            buf->start= buf->buffer;
            ssize_t r = read(fd,buf->buffer,buf->size);
            //printf("Read %d bytes\n",r);
            if (r==0) {
                buf->end=buf->start;
                return wrote;
            }
            buf->end=buf->start+r;
        }
    }
    
    
}

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>

int main() {
    int fp=open("/home/salvo/.bash_history",O_RDONLY);
    //int fp=open("/home/salvo/.xinitrc",O_RDONLY);
    buffered_read_t buffer; //=malloc(sizeof(buffered_read_t));
    buffer_init(&buffer,32);
    
    char* testo = malloc(5);
    ssize_t dim;
    while ((dim=buffer_read(fp, testo, 4, &buffer))<4) {
        testo[dim]=0;
        //printf("Letti %d: %s\n",dim,testo);
        printf("%s",testo);
    }
    
    buffer_free(&buffer);
}