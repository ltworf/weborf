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

#include "options.h"

#include <stdlib.h>
#include <string.h>

#include "dict.h"

/**
 * Initializes the data structure for
 * the dictionary.
 *
 * size: maximum amount of items allowed
 *
 * returns 0 in case of correct execution
 */
int dict_init(dict_t *dic,size_t size) {
    dic->size=size;
    dic->items=0;
    dic->key=calloc(sizeof(char*),size);
    dic->value=calloc(sizeof(char*),size);

    if (dic->key==NULL || dic->value==NULL) {
        dict_free(dic);
        return -1;
    }
    return 0;
}

/**
 * Frees the memory allocated by the dictionary
 * It only frees the pointers to the keys and values,
 * not the keys and values themselves.
 */
void dict_free(dict_t *dic) {
    free(dic->key);
    free(dic->value);
    dic->size=0;
    dic->items=0;
}

char *dict_get_key(dict_t *dic,char *key) {
    unsigned int i=0;
    while (i < dic->items && dic->key[i]!=NULL) {
        if (strcmp(key,dic->key[i])==0)
            return dic->value[i];
        i++;
    }
    return NULL;
}

/**
 * Adds a pair to the dictionary.
 * The pair will be added, and no check for duplicates
 * will be performed.
 *
 * dict presumes the pointers to the strings will remain valid
 * no copy is performed.
 *
 * returns 0 on correct insertion
 */
int dict_add_pair(dict_t *dic,char *key, char *value) {
    if (dic->items>=dic->size) return -1;

    dic->key[dic->items]=key;
    dic->value[dic->items]=value;

    dic->items+=1;

    return 0;
}
// kate: indent-mode cstyle; space-indent on; indent-width 0;
