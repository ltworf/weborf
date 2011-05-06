/*
Weborf
Copyright (C) 2011  Salvo "LtWorf" Tomaselli

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

#ifndef WEBORF_DICT_H
#define WEBORF_DICT_H


typedef struct {
    char **key;
    char **value;
    size_t size;
    size_t items;
} dict_t;



int dict_init(dict_t *dic,size_t size);
void dict_free(dict_t *dic);
char *dict_get_key(dict_t *dic,char *key);
int dict_add_pair(dict_t *dic,char *key,char *value);

#endif
