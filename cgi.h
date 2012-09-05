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

#ifndef WEBORF_CGI_H
#define WEBORF_CGI_H


#include "options.h"
#include "types.h"
#include "connection_types.h"

#define STDIN 0
#define STDOUT 1
#define STDERR 2

void cgi_exec_page(char * executor,connection_t* connection_prop);
void cgi_free_resources(connection_t* connection_prop);
void cgi_wait_headers(connection_t* connection_prop);

#endif