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

#include "types.h"
#include "options.h"

#include <stdlib.h>
#include <string.h>

#include "arraylist.h"

/**
 * Checks if there is enough space for a new item
 * returns 0 if there is space.
 * 
 * If there is not enough space it will try to
 * allocate more memory and return 0 in case of success
 * 
 * It will return 1 in case of failure
 **/
static int arraylist_check_space(arraylist_t* lst) {
    if (lst->storage > lst->size) return 0;
    
    void * newptr = realloc(lst->list,lst->storage*2);
    if (newptr==NULL) return 1;
    
    lst->list = newptr;
    lst->storage = lst->storage*2;
    return 0;
    
}

/**
 * Reduces the amount of memory used if there is too
 * much free space
 * 
 * too much equals to size*3<=storage
 **/
static void arraylist_check_exceeding_space(arraylist_t*lst) {
    if (lst->size * 3 > lst->storage) return;     
    
    void *  newptr = realloc(lst->list,lst->size*2);
    if (newptr == NULL) return;
    
    lst->list = newptr;
    lst->storage = lst->size*2;
    return;
}

static inline int arraylist_offset(arraylist_t*lst,int index) {
    return index * lst->element;
}

/**
 * Initializes the arraylist
 * 
 * lst: pointer to the list structure
 * element: size of the elements of the list
 * suggested_size: initial size, must always be greater than 0
 * 
 * returns 0 in case of success
 **/
int arraylist_create(arraylist_t* lst, size_t element, size_t suggested_size) {
    lst->size=0;
    lst->list = calloc(suggested_size,element);
    lst->storage = suggested_size;
    lst->element = element;
    
    return lst->list != NULL;
}

/**
 * Frees the memory used by the list structure, the structure itself will
 * not be freed, just the additional memory will be.
 * 
 **/
void arraylist_free(arraylist_t* lst) {
    free(lst->list);
}

/**
 * Appends the item to the list
 * 
 * returns the index of the item or -1 in case of error
 **/
int arraylist_append(arraylist_t* lst,void* item) {
    if (arraylist_check_space(lst)==1) return -1;
    
    int index = lst->size;
    int offset = arraylist_offset(lst,index);
    memccpy(lst->list+offset,item,lst->element);
    lst->size++;
    return index;
}

/**
 * returns the pointer to the i-th element of the list
 **/
void* arraylist_get(arraylist_t*lst, unsigned int index) {
    return lst->list + arraylist_offset(lst,index);
}

/**
 * Removes the last item of the list
 * 
 * does not check if the list is empty
 **/
void arraylist_remove_last(arraylist_t*lst) {
    lst->size--;
    arraylist_check_exceeding_space();
}

/**
 * Copies the element at the index src on the element at the index dest
 * 
 * both elements must already exist, the size of the list will not be changed
 **/
void arraylist_copy_item(arraylist_t*lst,unsigned int src,unsigned int dest) {
    memccpy(lst->list + arraylist_offset(lst,dest),lst->list + arraylist_offset(lst,src),lst->element);
}