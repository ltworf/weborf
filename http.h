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

#ifndef WEBORF_HTTP_H
#define WEBORF_HTTP_H


#include "types.h"

#include "configuration.h"
#include "connection_types.h"
#include "mystring.h"
#include "utils.h"

#include <string.h>
#include <time.h>


static inline void http_set_chunked(connection_t * connection_prop);
static inline int http_set_connection_t(char* header,connection_t * connection_prop);
static inline char *http_reason_phrase(int code);

void http_append_header_str(connection_t * connection_prop,const char* s,const char*);
void http_append_header_str_str(connection_t * connection_prop,const char* s,char* s1,char* s2);
void http_append_header_safe(connection_t * connection_prop,char* s);
void http_append_header_d(connection_t * connection_prop,const char* s, int d);
void http_append_header_sizet(connection_t * connection_prop,const char* s, size_t d);
void http_append_header_struct_tm(connection_t * connection_prop, const char* s, const struct tm *ts);
void http_append_header(connection_t * connection_prop,const char* s);
void http_append_header_llu_llu_lld(connection_t * connection_prop, const char* s,unsigned long long int s1, unsigned long long int s2, long long int s3);

ssize_t http_read_content_length(connection_t * connection_prop);
time_t http_read_if_none_match(connection_t * connection_prop);
time_t http_read_if_range(connection_t * connection_prop);
bool http_read_deep(connection_t * connection_prop);
bool http_read_range(connection_t * connection_prop, size_t *from, size_t *to);

/**
 * Defines the Connection header
 * It will set the connection header if the setting is non-default
 * Ie: will send keep alive if keep-alive is enabled and protocol is not 1.1
 * And will send close if keep-alive isn't enabled and protocol is 1.1
 **/
static inline void http_set_connection_header(connection_t * connection_prop) {
    if (connection_prop->protocol_version!=HTTP_1_1 && connection_prop->response.keep_alive==true) {
        http_append_header(connection_prop,"Connection: Keep-Alive\r\n");
    } else if (connection_prop->protocol_version==HTTP_1_1 && connection_prop->response.keep_alive==false) {
        http_append_header(connection_prop,"Connection: close\r\n");
    }
}


/**
 * This function returns the reason phrase according to the response
 * code.
 **/
static inline char *http_reason_phrase(int code) {
    code=code/100;

    switch (code) {
    case 2:
        return "OK";
    case 3:
        return "Found";
    case 4:
        return "Request error";
    case 5:
        return "Server error";
    case 1:
        return "Received";

    };
    return "Something is very wrong";
}

/**
 * Sets the fields in connection_t
 *
 * gets the HTTP header in a NULL terminated string
 * and a pointer to an initialized connection_t
 *
 * returns 0 on success and -1 if no known method was found.
 **/
static inline int http_set_connection_t(char* header,connection_t * connection_prop) {

    connection_prop->response.status_code = HTTP_CODE_NONE;
    connection_prop->response.chunked=false; //Always false by default
    connection_prop->response.timestamp=-1;
    connection_prop->response.headers.len = 0;
    connection_prop->response.headers.data[0] = 0;
    connection_prop->post_data.data=NULL;
    connection_prop->post_data.len=0;
    connection_prop->strfile_fd = -1;

    char *lasts;
    bool found = true;

    switch (header[0]) {
    case 'G':
        connection_prop->request.method_id=GET;
        break;
    case 'O':
        connection_prop->request.method_id=OPTIONS;
        break;
    case 'D':
        connection_prop->request.method_id=DELETE;
        break;
    case 'P': {
        switch (header[1]) {
        case 'O':
            connection_prop->request.method_id=POST;
            break;
        case 'U':
            connection_prop->request.method_id=PUT;
            break;
#ifdef WEBDAV
        case 'R':
            connection_prop->request.method_id=PROPFIND;
            break;
#endif
        default:
            found=false;
        };
    }
    break;
#ifdef WEBDAV
    case 'C':
        connection_prop->request.method_id=COPY;
        break;
    case 'M': {
        switch (header[1]) {
        case 'K':
            connection_prop->request.method_id=MKCOL;
            break;
        case 'O':
            connection_prop->request.method_id=MOVE;
            break;
        default:
            found=false;
        }
    }
    break;
#endif
    default:
        found=false;
    };

    if (found == false) return -1;

    connection_prop->method=strtok_r(header," ",&lasts);//Must be done to eliminate the request
    connection_prop->page=strtok_r(NULL," ",&lasts);
    if (connection_prop->page==NULL || connection_prop->method == NULL) return -1;

    connection_prop->http_param=lasts;

    {
        char a[12];
        //Obtains the connection header, writing it into the a buffer, and sets connection=true if the header is present
        bool connection=get_param_value(connection_prop->http_param,"Connection", a,sizeof(a),strlen("Connection"));

        //Setting the connection type, using protocol version
        if (lasts[7]=='1' && lasts[5]=='1') {//Keep alive by default (protocol 1.1)
            connection_prop->protocol_version=HTTP_1_1;
            connection_prop->response.keep_alive=(connection && strncmp(a,"close",5)==0)?false:true;
        } else {//Not http1.1
            connection_prop->protocol_version=lasts[7]; //Constants are set to make this line work
            connection_prop->response.keep_alive=(connection && strncmp(a,"Keep",4)==0)?true:false;
        }
    }

    {
        //Remove escapes from URL, WARNING prevents directory traversal
        char* url=connection_prop->page;
        replaceEscape(url);
        strReplace(url,"../",'\0');
        //TODO AbsoluteURI: Check if the url uses absolute url, and in that case remove the 1st part
    }

    {
        //Split the URI into  a page and the following parameters
        char *separator=strstr(connection_prop->page,"?");

        if (separator==NULL) {
            connection_prop->get_params=NULL;
            connection_prop->page_len=strlen(connection_prop->page);
        } else {
            separator[0]=0;
            connection_prop->get_params=separator+1;
            connection_prop->page_len=separator-connection_prop->page;
        }
    }

    connection_prop->basedir=configuration_get_basedir(connection_prop->http_param);

    connection_prop->strfile_len = snprintf(connection_prop->strfile,
                                            URI_LEN,
                                            "%s%s",
                                            connection_prop->basedir,
                                            connection_prop->page);

    return 0;
}

/**
 * Used when the size of the response is unknown,
 * it tries to set chunked encoding,
 * otherwise it falls back to removing the keep alive.
 *
 * If chuncked is set, the appropriate header is set too
 **/
static inline void http_set_chunked(connection_t * connection_prop) {
    if (connection_prop->protocol_version == HTTP_1_1) {
        connection_prop->response.chunked=true;
        http_append_header(connection_prop,"Transfer-Encoding: chunked\r\n");
    } else {
        connection_prop->response.keep_alive=false;
    }
}




#endif