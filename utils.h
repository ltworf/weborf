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
*/

#ifndef WEBORF_UTILS_H
#define WEBORF_UTILS_H

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>


#include "mystring.h"
#include "options.h"
#include "types.h"

bool file_exists(char *file);
int list_dir(connection_t *connection_prop, char *html, unsigned int bufsize, bool parent);
void help();
void version();
void moo();
void setEnvVars(char *http_param);
bool get_param_value(char *http_param, char *parameter, char *buf, ssize_t size,ssize_t param_len);
int deep_rmdir(char * dir);

#ifdef WEBDAV
int dir_move_copy (char* source, char* dest,int method);
int file_copy(char* source, char* dest);
int file_move(char* source, char* dest);
int dir_move (char* source, char* dest);
int dir_copy (char* source, char* dest);
#endif

#endif
