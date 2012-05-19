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
    STATUS_WAIT_HEADER,
    STATUS_READY_TO_SEND,
    STATUS_END,
    STATUS_ERR,
    STATUS_ERR_NO_CONNECTION,
    STATUS_READY_FOR_NEXT,
    STATUS_NONE,

} conection_status_e;

typedef struct {
    size_t len;                //length of the string
    char *data;                 //Pointer to string
} string_t;

typedef struct {
    long int id;                //ID of the thread
    magic_t mime_token;         //Token for libmagic
} thread_prop_t;

typedef struct {
    size_t len;                //length of the array
    char *data[MAXINDEXCOUNT];  //Array containing pointers
    int data_l[MAXINDEXCOUNT];  //Array containing len of strings
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

    bool keep_alive;            //True if we are using pipelining
    unsigned int status_code;   //HTTP status code
} response_t;

typedef struct {

    int method_id;              //Index of the http method used (GET, POST)

} request_t;

typedef struct {
    int sock;                   //File descriptor for the socket

#ifdef IPV6
    char ip_addr[INET6_ADDRSTRLEN];              //ip address in string format
#else
    char ip_addr[INET_ADDRSTRLEN];
#endif

    short int protocol_version; //See defines like HTTP_something
    char *method;               //String version of the http method used
    char *http_param;           //Param string
    char *page;                 //Requested URI
    size_t page_len;            //Lengh of the page string
    char *get_params;           //Params in the URI, after the ? char
    char *strfile;              //File on filesystem
    size_t strfile_len;         //Length of string strfile
    struct stat strfile_stat;   //Stat of strfile
    int strfile_fd;             //File descriptor for strfile
    char *basedir;              //Basedir for the host
    response_t response;
    request_t request;
    conection_status_e status;  //Connection status

    buffered_read_t read_b;     //Buffer for buffered reader
    string_t buf;               //Buffer to read headers
} connection_t;

typedef struct {
    pthread_mutex_t mutex;      //Mutex to access this struct
    unsigned int free;          //Free threads
    unsigned int count;         //thread count
} t_thread_info;


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

    char *indexes[MAXINDEXCOUNT];//List of pointers to index files
    int indexes_l;              //Count of the list
    dict_t vhosts;         //virtual hosts

} weborf_configuration_t;

#endif

