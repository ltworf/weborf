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
#include "mystring.h"

#include <string.h>


static inline void http_set_chunked(connection_t * connection_prop);
static inline void http_unescape_url(char* url);
static inline int http_set_method_uri(char* header,connection_t * connection_prop);


/**
 * Sets the fields
 * method_id, method, page in connection_prop.
 *
 * returns 0 on success and -1 if no known method was found.
 **/
static inline int http_set_method_uri(char* header,connection_t * connection_prop) {
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
    return 0;
}



/**
 * Removes the escape sequences in the URL with
 * the real values.
 *
 * WARNING: also removes the "../" sequence to
 * prevent directory traversal
 **/
static inline void http_unescape_url(char* url) {
    replaceEscape(url);

    //Prevents the use of .. to access the whole filesystem
    strReplace(url,"../",'\0');

    //TODO AbsoluteURI: Check if the url uses absolute url, and in that case remove the 1st part
}

/**
 * Used when the size of the response is unknown,
 * it tries to set chunked encoding,
 * otherwise it falls back to removing the keep alive.
 **/
static inline void http_set_chunked(connection_t * connection_prop) {
    if (connection_prop->protocol_version == HTTP_1_1) {
        connection_prop->response.chunked=true;
    } else {
        connection_prop->response.keep_alive=false;
    }
}




#endif