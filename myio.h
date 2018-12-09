/*
Weborf
Copyright (C) 2010-2018 Salvo "LtWorf" Tomaselli

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

#ifndef WEBORF_MYIO_H
#define WEBORF_MYIO_H

#include "types.h"
#include "options.h"

#ifdef HAVE_LIBSSL
int myio_write(fd_t fd, const void *buf, size_t count);
int myio_read(fd_t fd, void *buf, size_t count);
static inline int myio_getfd(fd_t fd) { return fd.fd; }
static inline fd_t fd2fd_t(int fd) {
    fd_t r;
    r.ssl = NULL;
    r.fd = fd;
    return r;
}
#else
#define myio_read read
#define myio_write write
static inline int myio_getfd(fd_t fd) { return fd; }
static inline fd_t fd2fd_t(int fd) { return fd; }
#endif

int fd_copy(fd_t from, fd_t to, off_t count);
int dir_remove(char * dir);
bool file_exists(char *file);

#ifdef WEBDAV
int dir_move_copy (char* source, char* dest,int method);
int file_copy(char* source, char* dest);
int file_move(char* source, char* dest);
int dir_move (char* source, char* dest);
int dir_copy (char* source, char* dest);
#endif

#endif
