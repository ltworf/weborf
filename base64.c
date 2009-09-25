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

#include <stdlib.h>
#include <stdio.h>
#include "base64.h"

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
        res[3] = 0;

    }
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
Returns the base64 (6bit) code of a char
*/
char getCode(char c) {
    static char encodingTable[64] = {
        'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
        'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
        'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
        'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
        '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/'
    };

    int i;
    for (i = 0; i < 64; i++) {
        if (encodingTable[i] == c)
            return i;
    }
    return 0;
}
