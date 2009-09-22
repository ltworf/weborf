/*
Weborf
Copyright (C) 2007  Salvo "LtWorf" Tomaselli

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

#define _GNU_SOURCE


#ifndef O_LARGEFILE		//Needed to compile on Mac, where this doesn't exist
#define O_LARGEFILE 0
#endif

#include <sys/un.h>

#include <time.h>
#include <netinet/in.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <syslog.h>		//To use syslog
#include "options.h"
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdbool.h>		//Adds boolean type
#include <string.h>

#include "buffered_reader.h"

//Request
#define INVALID -1
#define GET 0
#define POST 1

//Errors
#define ERR_NOTHTTP -6
#define ERR_NONAUTH -5
#define ERR_SOCKWRITE -4
#define ERR_NOMEM -3
#define ERR_FILENOTFOUND -2
#define ERR_BRKPIPE -1

//Protocol version
#define HTTP_0_9 0
#define HTTP_1_0 1
#define HTTP_1_1 2


typedef struct {
    ssize_t len;		//length of the string
    char *data;			//Pointer to string
} string_t;

typedef struct {
    char *ip_addr;		//Pointer to ip address
    bool keep_alive;		//True if we are using pipelining
    short int protocol_version;
    int method_id;		//Index of the http method used
    char *method;		//String version of the http method used
    char *http_param;		//Param string
    char *page;			//Requested URI
    ssize_t page_len; //Lengh of the page string
    char *get_params;		//Params in the URI, after the ? char
    char *strfile;		//File on filesystem
    ssize_t strfile_len;	//Length of string strfile
    struct stat strfile_stat;	//Stat of strfile
    int strfile_fd;		//File descriptor for strfile
} connection_t;


int write_dir(int sock, char *real_basedir,
              connection_t * connection_prop);
void *instance(void *);
int send_page(int sock, buffered_read_t * read_b,
              connection_t * connection_prop);
int write_file(int sock, connection_t * connection_prop);
#ifdef __COMPRESSION
int write_compressed_file(int sock, unsigned int size, time_t timestamp,
                          connection_t * connection_prop);
#endif
int exec_page(int sock, char *executor, string_t * post_param,
              char *real_basedir, connection_t * connection_prop);
int send_err(int sock, int err, char *descr, char *ip_addr);
int send_http_header_scode(int sock, char *code, unsigned int size,
                           char *headers);
void piperr();
void modURL(char *url);
int request_auth(int sock, char *descr);
int check_auth(int sock, connection_t * connection_prop);
string_t read_post_data(int sock, connection_t * connection_prop,
                        buffered_read_t * read_b);
char *get_basedir(char *http_param);
void handle_requests(int sock, char *buf, buffered_read_t * read_b,
                     int *bufFull, connection_t * connection_prop,
                     long int id);
int send_http_header_full(int sock, int code, unsigned int size,
                          char *headers, bool content, time_t timestamp,
                          connection_t * connection_prop);
#endif
