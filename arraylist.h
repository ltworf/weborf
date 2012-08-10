/*
Weborf
Copyright (C) 2012  Salvo "LtWorf" Tomaselli

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


#ifndef WEBORF_ARRAYLIST
#define WEBORF_ARRAYLIST

#include "types.h"
#include "options.h"

typedef struct {
    size_t size;
    size_t storage;
    size_t element;
    void * list;
} arraylist_t;

int arraylist_create(arraylist_t* lst, size_t element, size_t suggested_size);
void arraylist_free(arraylist_t* lst);
int arraylist_append(arraylist_t* lst,void* item);
void* arraylist_get(arraylist_t*lst, unsigned int index);
void arraylist_remove_last(arraylist_t*lst);
void arraylist_copy_item(arraylist_t*lst,unsigned int src,unsigned int dest);

/**
 * Returns the number of elements currently in the list
 **/
static inline size_t arraylist_size(arraylist_t* lst) {
    return lst->size;
}



#endif