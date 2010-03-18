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

#include "mystring.h"

/**
This function converts a string to upper case
*/
void strToUpper(char *str) {
    int i = -1;
    while (str[++i]) {
        str[i] = toupper(str[i]);
    }
}

/**
This function splits the page name from the GET params.
It also sets the value for the page_len field
*/
void split_get_params(connection_t* connection_prop) {
    int i=0;
    connection_prop->get_params=NULL;
    while (connection_prop->page[i]!=0) {
        if (connection_prop->page[i]=='?') {
            connection_prop->page[i]=0;
            connection_prop->get_params=&connection_prop->page[i+1];
            break;
        }
        i++;
    }
    connection_prop->page_len=i;
}


/**
Replaces escape sequences in the form %HEXCODE with the correct char
This is used for URLs, after the transformation the URL will probably
represent a file that exists on filesystem.
Since after this replace the string will be unchanged or shorter, no
additional buffer will be needed.

This function is in-place, doesn't create copies but changes the original string.
*/
void replaceEscape(char *string) {
    char e_seq[3];
    e_seq[2] = 0;

    //Parses the string
    while ((string=strstr(string,"%"))!=NULL) {
        e_seq[0] = string[1];
        e_seq[1] = string[2];

        delChar(string, 0, 2);  //Deletes 2 chars from the url

        //Replaces the 3rd character with the char corresponding to the escape
        string[0] = strtol(e_seq, NULL, 16);
    }
}

/**
This function replaces, within the string string, the substring substr with the char with.

This function is in-place, doesn't create copies but changes the original string.
*/
void strReplace(char *string, char *substr, char with) {
    char *pointer;
    int substrlen = strlen(substr);

    while ((pointer = strstr(string, substr)) != NULL) {
        delChar(pointer, 0, substrlen - 1);
        pointer[0] = with;
        string=pointer;
    }
}

/**
This function deletes n chars from the string, starting from the position pos.
In case deleting of more chars than the string's len itself, the string will be returned unchanged.

This function is in-place, doesn't create copies but changes the original string.
*/
void delChar(char *string, int pos, int n) {
    /*
    This isn't using strcpy because here the strings overlap.
    memmove isn't good too because
    */
    if (strlen(string+pos) < n) { //String is long enough
        return;
    }

    char *c1, *c2;
    for (c1 = string + pos, c2 = c1 + n; *c2 != 0; c1++, c2++) {
        *c1 = *c2;
    }
    *c1 = 0;
}



/**
This function changes param's separator from & to \0, to create several strings.
Returns number of strings
*/
int splitParams(char *string) {
    int params = 0;
    int i;
    int to = strlen(string);

    for (i = 0; i < to; i++) {
        if (string[i] == '&') {
            string[i] = '\0';
            params++;
        }
    }
    return params + 1;
}

/**
Returns true if str ends with end

str String to compare
end second string. If str ends with end, true is returned
false otherwise
*/
bool endsWith(char *str, char *end,ssize_t len_str,ssize_t len_end) {
    int i;

    for (i = 1; i <= len_end; i++) {
        if (end[len_end - i] != str[len_str - i])
            return false;
    }

    return true;
}

/**
Removes cr lf chars from the beginning of the string

This function is in-place, doesn't create copies but changes the original string.
*/
int removeCrLf(char *buf) { //Removing initial \n or \r
    int count = 0;
    while (buf[count] == 10 || buf[count] == 13)
        count++;
    delChar(buf, 0, count);
    return count;
}
