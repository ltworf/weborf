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

#include <stdio.h>
#include <stdlib.h>

#include "http.h"

static string_t * get_string_t(connection_t * connection_prop);

static string_t * get_string_t(connection_t * connection_prop) {
    //TODO realloc the header if it's not enough
    return  & (connection_prop->response.headers);

}

/**
 * Appends an header
 *
 * WARNING: appending string containing things like %d
 * could result in a security attack. Use str_append_str_safe
 * in case the content is not sanitized
 **/
void http_append_header(connection_t * connection_prop,const char* s) {
    string_t * string = get_string_t(connection_prop);

    char *head=string->data+string->len;

    string->len += snprintf(head,HEADBUF-string->len,s);
}

/**
 * Appends an header.
 * Being safe means that the string might contain % sequences and that
 * will not create security problems.
 * 
 * Use this function with values coming from the outside.
 **/
void http_append_header_safe(connection_t * connection_prop,char* s) {
    string_t * string = get_string_t(connection_prop);

    char *head=string->data+string->len;

    string->len += snprintf(head,HEADBUF-string->len,"%s",s);
}

/**
 * Appends an header with a string in it
 *
 * WARNING: the header must contain one and only one %s
 **/
void http_append_header_str(connection_t * connection_prop,const char* s,char* s1) {
    string_t * string = get_string_t(connection_prop);
    char *head=string->data+string->len;
    string->len += snprintf(head,HEADBUF-string->len,s,s1);
}

void http_append_header_int(connection_t * connection_prop,const char* s,int d) {
    string_t * string = get_string_t(connection_prop);
    char *head=string->data+string->len;
    string->len += snprintf(head,HEADBUF-string->len,s,d);
}

void http_append_header_str_str(connection_t * connection_prop,const char* s,char* s1,char* s2) {
    string_t * string = get_string_t(connection_prop);
    char *head=string->data+string->len;
    string->len += snprintf(head,HEADBUF-string->len,s,s1,s2);
}

void http_append_header_llu_llu_lld(connection_t * connection_prop, const char* s,unsigned long long int s1, unsigned long long int s2, long long int s3) {
    string_t * string = get_string_t(connection_prop);
    char *head=string->data+string->len;
    string->len += snprintf(head,HEADBUF-string->len,s,s1,s2,s3);
}


/**
 * Reads the Content-Length header
 *
 * returns -1 if the header has an error
 **/
ssize_t http_read_content_length(connection_t * connection_prop) {
    char *content_length="\nContent-Length: ";
    char *val = strstr(connection_prop->http_param, content_length);

    if (val == NULL) { //No such field
        return -1;
    }

    //WARNING messing with this line must be done carefully
    val += strlen(content_length); //Moves the begin of the string to exclude the name of the field
    return strtoull(val, NULL, 0);
}

/**
 * Reads the header If-None-Match
 * returning a time_t, since weborf uses the mtime as etag
 *
 * returns -1 on error
 **/
time_t http_read_if_none_match(connection_t * connection_prop) {
    char *if_none_match="\nIf-None-Match: ";
    char *val = strstr(connection_prop->http_param, if_none_match);

    if (val == NULL || val[strlen(if_none_match)]==0) { //No such field
        return -1;
    }

    //WARNING messing with this line must be done carefully
    val += strlen(if_none_match) + 1; //Moves the begin of the string to exclude the name of the field
    return (time_t)strtol(val,NULL,0);
}

time_t http_read_if_range(connection_t * connection_prop) {
    char *if_none_match="\nIf-Range: ";
    char *val = strstr(connection_prop->http_param, if_none_match);

    if (val == NULL || val[strlen(if_none_match)]==0) { //No such field
        return -1;
    }

    //WARNING messing with this line must be done carefully
    val += strlen(if_none_match) + 1; //Moves the begin of the string to exclude the name of the field
    return (time_t)strtol(val,NULL,0);
}

/**
 * Reads the Depth header
 * returns false in all cases except when it begins with '1'
 **/
bool http_read_deep(connection_t * connection_prop) {
    char *depth = "\nDepth: ";
    char *val = strstr(connection_prop->http_param,depth);

    if (val == NULL) return false;

    //WARNING messing with this line must be done carefully
    val += strlen(depth); //Moves the begin of the string to exclude the name of the field

    return val[0]=='1';

}

/**
 * Parses the Range header, setting start and end
 *
 * the end could be 0 if it was unset by the client
 *
 * returns false if the header was not found
 **/
bool http_read_range(connection_t * connection_prop, size_t *from, size_t *to) {
    char *range="\nRange: ";

    char *val = strstr(connection_prop->http_param,range);
    if (val==NULL) return false;

    val+=strlen(range);
    char *end = strstr(val,"\r\n");
    if (end==NULL) return false;

    end[0]=0;
    //Locating from and to
    //Range: bytes=12323-123401
    char *eq=strstr(val,"=");
    char *sep=strstr(val,"-");
    end[0]='\r';

    //Invalid data in Range header
    if (eq==NULL ||sep==NULL) return false;

    sep[0]=0;
    *from=strtoull(eq+1,NULL,0);
    *to=strtoull(sep+1,NULL,0);
    sep[0]='-';
    return true;
}
