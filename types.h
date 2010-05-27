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

#include <stdbool.h> //Adds boolean type
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <netinet/in.h>


#include "options.h"


typedef struct {
    ssize_t len;                //length of the array
    char *data[MAXINDEXCOUNT];  //Array containing pointers
    int data_l[MAXINDEXCOUNT];  //Array containing len of strings
} array_ll;

typedef struct {
    int num, size;                //Filled positions in the queue, and its maximum size
    int head, tail;               //pointers to head and tail of the round queue
    int *data;                    //Socket with client

#ifdef IPV6
    struct sockaddr_in6 *addr;    //Local and remote address
#else
    struct sockaddr_in *addr;
#endif

    pthread_mutex_t mutex;        //mutex to modify the queue
    pthread_cond_t for_space, for_data;
    int n_wait_sp, n_wait_dt;
} syn_queue_t;

typedef struct {
#ifdef IPV6
    char ip_addr[INET6_ADDRSTRLEN];              //ip address in string format
#else
    char ip_addr[INET_ADDRSTRLEN];
#endif

    bool keep_alive;            //True if we are using pipelining
    short int protocol_version; //See defines like HTTP_something
    int method_id;              //Index of the http method used (GET, POST)
    char *method;               //String version of the http method used
    char *http_param;           //Param string
    char *page;                 //Requested URI
    ssize_t page_len;           //Lengh of the page string
    char *get_params;           //Params in the URI, after the ? char
    char *strfile;              //File on filesystem
    ssize_t strfile_len;        //Length of string strfile
    struct stat strfile_stat;   //Stat of strfile
    int strfile_fd;             //File descriptor for strfile
} connection_t;

typedef struct {
    ssize_t len;                //length of the string
    char *data;                 //Pointer to string
} string_t;

typedef struct {
    pthread_mutex_t mutex;          //Mutex to access this struct
    unsigned int free;              //Free threads
    unsigned int count;             //thread count
} t_thread_info;

#endif