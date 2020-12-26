/*
Weborf
Copyright (C) 2007-2020  Salvo "LtWorf" Tomaselli

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
@author Salvo Rinaldi <salvin@anche.no>
 */

#ifndef WEBORF_INSTANCE_H
#define WEBORF_INSTANCE_H

#ifndef O_LARGEFILE //Needed to compile on Mac, where this doesn't exist
#define O_LARGEFILE 0
#endif

#include "types.h"
#include "buffered_reader.h"

#ifdef WEBDAV
#include "webdav.h"
#endif


//Request
#define INVALID -1
#define GET 0
#define POST 1
#define PUT 2
#define DELETE 3
#define OPTIONS 8

#ifdef WEBDAV
#define PROPFIND 4
#define COPY 5
#define MOVE 6
#define MKCOL 7
#endif


#define NO_ACTION -120

//Errors
#define ERR_RANGE_NOT_SATISFIABLE -15
#define ERR_PRECONDITION_FAILED -14
#define ERR_NOT_ALLOWED -13
#define ERR_INSUFFICIENT_STORAGE -12
#define ERR_CONFLICT -11
#define ERR_SERVICE_UNAVAILABLE -10
#define ERR_FORBIDDEN -9
#define ERR_NOTIMPLEMENTED -8
#define ERR_NODATA -7
#define ERR_NOTHTTP -6
#define ERR_NOAUTH -5
#define ERR_SOCKWRITE -4
#define ERR_NOMEM -3
#define ERR_FILENOTFOUND -2
#define ERR_BRKPIPE -1

//Ok
#define OK_CREATED 1
#define OK_NOCONTENT 2

//Protocol version
#define HTTP_0_9 57
#define HTTP_1_0 48
#define HTTP_1_1 2

void inetd();
void *instance(void *);
int write_file(connection_t * connection_prop);
int send_err(connection_t *connection_prop,int err,char* descr);
string_t read_post_data(connection_t * connection_prop, buffered_read_t * read_b);
char *get_basedir(char *http_param);
int send_http_header(int code, unsigned long long int *size, char *headers, bool content, time_t timestamp, connection_t * connection_prop);
int delete_file(connection_t* connection_prop);
int read_file(connection_t* connection_prop,buffered_read_t* read_b);
#endif
