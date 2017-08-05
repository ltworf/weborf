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

#ifndef WEBORF_MYSTRING_H
#define WEBORF_MYSTRING_H

#include "types.h"

void split_get_params(connection_t* connection_prop);
bool endsWith(char *str, char *end, size_t len_str, size_t len_end);
void delChar(char *string, int pos, int n);
void strReplace(char *string, char *substr, char with);
void replaceEscape(char *string);
void strToUpper(char *str);

#endif
