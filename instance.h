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
@author Salvo Rinaldi <salvin@bluebottle.com>
 */

#ifndef WEBORF_INSTANCE_H
#define WEBORF_INSTANCE_H

#define _GNU_SOURCE

#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <syslog.h>	//To use syslog
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
#include <stdbool.h>	//Adds boolean type
#include <string.h>


#define INVALID -1
#define GET 0
#define POST 1


#define ERR_SOCKWRITE -4
#define ERR_NOMEM -3
#define ERR_FILENOTFOUND -2
#define ERR_BRKPIPE -1

void * instance(void *);
int sendPage(int sock,char * page,char * http_param,int method_id,char * method,char* ip_addr);
int writePage(int sock,char * file);
int execPage(int sock, char * file, char * params,char * executor,char * http_param,char* post_param,char * method);
int send_err(int sock,int err,char* descr,char* ip_addr);
int send_http_header(int sock,unsigned int size);
void piperr();
void modURL(char* url);
int request_auth(int sock,char* descr);

#endif
