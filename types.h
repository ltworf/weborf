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
    HTTP_CODE_INTERNAL_SERVER_ERROR = 500,
    HTTP_CODE_PAGE_NOT_FOUND = 404,
    HTTP_CODE_SERVICE_UNAVAILABLE = 503,
    HTTP_CODE_BAD_REQUEST = 400,
    HTTP_CODE_FORBIDDEN = 403,
    HTTP_CODE_NOT_IMPLEMENTED = 501,
    HTTP_CODE_PRECONDITION_FAILED = 412,
    HTTP_CODE_CONFLICT = 409,
    HTTP_CODE_INSUFFICIENT_STORAGE = 507,
    HTTP_CODE_METHOD_NOT_ALLOWED = 405,
    HTTP_CODE_OK_NO_CONTENT = 204,
    HTTP_CODE_PARTIAL_CONTENT = 206,
    HTTP_CODE_OK_CREATED = 201,
    HTTP_CODE_OK = 200,
    HTTP_CODE_MOVED_PERMANENTLY = 301,
    HTTP_CODE_SEE_OTHER = 303,
    HTTP_CODE_NOT_MODIFIED = 304,
    HTTP_CODE_UNAUTHORIZED = 401,
    WEBDAV_CODE_MULTISTATUS = 207,
    HTTP_CODE_DISCONNECTED = 1000,
    HTTP_CODE_NONE = 0,
} http_response_codes_e;

typedef enum {
    STATUS_WAIT_HEADER,
    STATUS_READY_TO_SEND,
    STATUS_CHECK_AUTH,
    STATUS_INIT_CHECK_AUTH,
    STATUS_END,
    STATUS_ERR,
    STATUS_ERR_NO_CONNECTION,
    STATUS_READY_FOR_NEXT,
    STATUS_PAGE_SENT,
    STATUS_WAIT_DATA,
    STATUS_SERVE_REQUEST,
    STATUS_PUT_METHOD,
    STATUS_GET_METHOD,
    STATUS_SEND_HEADERS,
    STATUS_COPY_FROM_POST_DATA_TO_SOCKET,
    STATUS_TAR_DIRECTORY,
    STATUS_CGI_COPY_POST,
    STATUS_CGI_WAIT_HEADER,
    STATUS_CGI_SEND_CONTENT,
    STATUS_CGI_FREE_RESOURCES,
    STATUS_CGI_FLUSH_HEADER_BUFFER,

} conection_status_e;

typedef enum {
    HTTP_0_9 = 57,
    HTTP_1_0 = 48,
    HTTP_1_1 = 2,
} http_proto_version_t;

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

    bool keep_alive;            //True if we are using pipelining
    bool chunked;               //True if we are using chunked encoding
    http_response_codes_e status_code;   //HTTP status code
    time_t timestamp;           //Timestamp of the entity, set to -1 if unknown
    string_t headers;           //String for the extra-headers to be added on the flight
} response_t;

typedef struct {

    http_method_t method_id;              //Index of the http method used (GET, POST)

} request_t;

typedef struct {
    int sock;                   //File descriptor for the socket

#ifdef IPV6
    char ip_addr[INET6_ADDRSTRLEN];              //ip address in string format
#else
    char ip_addr[INET_ADDRSTRLEN];
#endif

    http_proto_version_t protocol_version; //HTTP protocol version used
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
    string_t post_data;         //Data of the POST
    response_t response;
    request_t request;
    conection_status_e status;  //Connection status
    conection_status_e status_next; //next status
    size_t bytes_to_copy;

    buffered_read_t read_b;     //Buffer for buffered reader
    string_t buf;               //Buffer to read headers

    int fd_to_cgi;              //File descriptor to write to the CGI
    int fd_from_cgi;            //File descriptor to read from the CGI
    string_t cgi_buffer;        //Buffer for the CGI headers
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

    array_ll indexes;//List of pointers to index files
    dict_t vhosts;         //virtual hosts

} weborf_configuration_t;

#endif

