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
#ifndef BUFFERED_READER_H
#define BUFFERED_READER_H

#include <unistd.h>
typedef struct {
    char *buffer;   //Buffer where the reader stores the read data
    char *start;    //Pointer to non-consumed data
    char *end;      //Pointer to 1st byte after end of the data. A read must continue after end.
    int size;       //Size of the buffer
} buffered_read_t;

void buffer_reset (buffered_read_t * buf);
short int buffer_init(buffered_read_t * buf, ssize_t size);
void buffer_free(buffered_read_t * buf);
ssize_t buffer_read(int fd, void *b, ssize_t count, buffered_read_t * buf);
#endif
