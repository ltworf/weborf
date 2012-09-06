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

#ifndef WEBORF_TYPES_H
#define WEBORF_TYPES_H

#include "options.h"

#include <stdbool.h> //Adds boolean type
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <netinet/in.h>
#include "dict.h"
#include "buffered_reader.h"

#ifdef SEND_MIMETYPES
#include <magic.h>
#else
typedef void* magic_t;
#endif



typedef enum {
    INVALID =  -1,
    GET = 0,
    POST = 1,
    PUT = 2,
    DELETE = 3,
    OPTIONS = 8,

    //Dav methods
    PROPFIND = 4,
    COPY = 5,
    MOVE = 6,
    MKCOL = 7,
} http_method_t;

typedef struct {
    size_t len;                //length of the string
    char *data;                 //Pointer to string
} string_t;

typedef struct {
    size_t len;                //length of the array
    char *data[MAXINDEXCOUNT];  //Array containing pointers
    size_t data_l[MAXINDEXCOUNT];  //Array containing len of strings
} array_ll;

typedef struct {
    int num, size;                //Filled positions in the queue, and its maximum size
    int head, tail;               //pointers to head and tail of the round queue
    int *data;                    //Socket with client
    pthread_mutex_t mutex;        //mutex to modify the queue
    pthread_cond_t for_space, for_data;
    int n_wait_sp, n_wait_dt;
} syn_queue_t;

typedef struct {

    http_method_t method_id;              //Index of the http method used (GET, POST)

} request_t;



typedef struct {
    char *basedir;
    char* authsock;             //Executable that will authenticate
    uid_t uid;                  //Uid to use after bind
#ifdef SEND_MIMETYPES
    bool send_content_type;     //True if we want to send the content type
#endif
    bool is_inetd;              //True if it expects to be executed by inetd
    array_ll cgi_paths;         //Paths to cgi binaries
    bool virtual_host;          //True if must check for virtual hosts
    bool exec_script;           //Enable CGI if false
    bool tar_directory;         //Sends directories compressed into tar-files
    char *ip;                   //IP addr with default value
    char *port;                 //port with default value

    array_ll indexes;//List of pointers to index files
    dict_t vhosts;         //virtual hosts

    int socket;            //Socket to listen

} weborf_configuration_t;

#endif

