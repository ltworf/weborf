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

bool file_exists(char * file);
char* findNext(char* str);
int list_dir (char* dir,char* html,unsigned int bufsize,bool parent);
int fileIsA(char* file);
void help();
void version();
void moo();
void setEnvVars(char * http_param,bool post);
bool get_param_value(char* http_param,char* parameter,char*buf,int size);

#endif
