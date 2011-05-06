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

void dict_free(dict_t *dic) {
    free(dic->key);
    free(dic->value);
    dic->size=0;
    dic->items=0;
}

char *dict_get_key(dict_t *dic,char *key) {
    unsigned int i=0;
    printf("get key %s\n",key);
    while (i < dic->items && dic->key[i]!=NULL) {
        if (strcmp(key,dic->key[i])==0)
            return dic->value[i];
        i++;
    }
    return NULL;
}

int dict_add_pair(dict_t *dic,char *key, char *value) {
    if (dic->items>=dic->size) return -1;

    dic->key[dic->items]=key;
    dic->value[dic->items]=value;

    dic->items+=1;

    int i;
    for (i=0; i<dic->items; i++)
        printf("key %s\tvalue %s\n",dic->key[i],dic->value[i]);

    return 0;
}
