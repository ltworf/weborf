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

#include "options.h"

#include <stdlib.h>

#include "buffered_reader.h"
#include "connection.h"


/**
 * Creates a new connection_t,
 * 
 * allocates its internal buffers:
 * buf.data
 * headers.data
 * strfile
 * read_b
 * 
 * Returns a pointer to the structure in case of
 * success
 * or NULL in case of failure
 **/
connection_t* connection_getnew() {
    connection_t * connection_prop = malloc(sizeof(connection_t));
    
    connection_prop->buf.data = calloc(INBUFFER+1,sizeof(char));
    connection_prop->buf.len = 0;

    connection_prop->response.headers.data = malloc(HEADBUF);
    connection_prop->response.headers.len = 0;
    connection_prop->response.status_code = HTTP_CODE_NONE;

    connection_prop->strfile=malloc(URI_LEN);    //buffer for filename

    if (buffer_init(&(connection_prop->read_b),BUFFERED_READER_SIZE)!=0 ||
           connection_prop->buf.data==NULL ||
           connection_prop->response.headers.data == NULL ||
           connection_prop->strfile==NULL) {
            connection_free(connection_prop);
            return NULL;
           }
    return connection_prop;

}

/**
 * Frees a connection_t structure
 **/
void connection_free(connection_t *connection_prop) {
    free(connection_prop->buf.data);
    free(connection_prop->strfile);
    free(connection_prop->response.headers.data);
    buffer_free(&(connection_prop->read_b));
    free(connection_prop);
}