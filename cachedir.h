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

#ifndef WEBORF_CACHEDIR_H
#define WEBORF_CACHEDIR_H


#include "types.h"

bool send_cached_item(unsigned int uprefix,connection_t* connection_prop);
int get_cached_item(unsigned int uprefix,connection_t* connection_prop);
void store_cache_item(unsigned int uprefix,connection_t* connection_prop, char *content, size_t content_len);
void init_cache(char *dir);
int clear_cache();

#endif