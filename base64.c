/*
Weborf
Copyright (C) 2007-2016  Salvo "LtWorf" Tomaselli

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
#include <stdio.h>
#include "base64.h"

/**
Returns the base64 (6bit) code of a char
*/
static char getCode(char c) {
    if ('A' <=c && c<='Z') {
        return c-'A';
    } else if ('a' <=c && c<='z') {
        return c-'a'+26;
    } else if ('0' <=c && c<='9') {
        return c-'0'+52;
    } else if (c=='+')
        return 62;
    else if(c=='/')
        return 63;
    return 0;
}

/**
Decodes a group of 4 characters and put 3 resulting chars in res
res buffer must be able to contain 3 bytes.
*/
void decode4_64(char *res, char *group) {

    ///////CALCULATE CHAR 0
    res[0] = getCode(group[0]) << 2;

    char t = getCode(group[1]);
    char t1 = t;

    t >>= 4;
    res[0] = res[0] | t;	//Completed 1st char

    ///////CALCULATE CHAR 1
    res[1] = t1 << 4;

    t = getCode(group[2]);
    t1 = t;

    res[1] = res[1] | (t >> 2);

    //CALCULATE CHAR 2
    res[2] = t1 << 6;
    t = getCode(group[3]);

    res[2] = res[2] | t;
}

/**
Decodes a base64 string, putting the result inside the result buffer.
Terminates with a \0 the buffer, so string functions can be used on the result.
Doesn't change the encoded string.
result's length must be at least: (strlen(encoded) /4 *3 )+1
*/
void decode64(char *result, char *encoded) {
    char *res = result;

    while (encoded[0] != 0) {
        decode4_64(res, encoded);
        encoded += 4;
        res += 3;
        res[0] = 0;

    }
}
