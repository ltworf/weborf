/*
Weborf
Copyright (C) 2010  Salvo "LtWorf" Tomaselli

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

#include <syslog.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <errno.h>


#include "mystring.h"
#include "cgi.h"
#include "http.h"
#include "types.h"
#include "instance.h"

extern char ** environ;                     //To reset environ vars
extern weborf_configuration_t weborf_conf;

/**
 * This function will set enviromental variables mapping the HTTP request.
 * Each variable will be prefixed with "HTTP_" and will be converted to
 * upper case.
*/
static inline void cgi_set_http_env_vars(char *http_param) { //Sets Enviroment vars
    if (http_param == NULL)
        return;

    char *lasts;
    char *param;
    int i;
    int p_len;

    //Removes the 1st part with the protocol
    param = strtok_r(http_param, "\r\n", &lasts);
    setenv("SERVER_PROTOCOL", param, true);

    char hparam[200];
    hparam[0] = 'H';
    hparam[1] = 'T';
    hparam[2] = 'T';
    hparam[3] = 'P';
    hparam[4] = '_';

    //Cycles parameters
    while ((param = strtok_r(NULL, "\r\n", &lasts)) != NULL) {

        p_len = lasts-param-1;
        char *value = NULL;

        //Parses the parameter to split name from value
        for (i = 0; i < p_len; i++) {
            if (param[i] == ':' && param[i + 1] == ' ') {
                param[i] = '\0';
                value = &param[i + 2];

                strToUpper(param); //Converts to upper case
                strReplace(param, "-", '_');
                memccpy(hparam+5,param,'\0',195);
                setenv(hparam, value, true);

                break;
            }
        }
    }
}

/**
 * Sets the SERVER_ADDR enviromental variable to be the string representation
 * of the SERVER IP ADDRESS which is being used by the socket
 * */
static inline void cgi_set_SERVER_ADDR_PORT(int sock) {

#ifdef IPV6
    char loc_addr[INET6_ADDRSTRLEN];
    struct sockaddr_in6 addr;
    socklen_t addr_l=sizeof(struct sockaddr_in6);

    getsockname(sock, (struct sockaddr *)&addr, &addr_l);
    inet_ntop(AF_INET6, &addr.sin6_addr,(char*)&loc_addr, INET6_ADDRSTRLEN);
#else
    char loc_addr[INET_ADDRSTRLEN];
    struct sockaddr_in addr;
    int addr_l=sizeof(struct sockaddr_in);

    getsockname(sock, (struct sockaddr *)&addr,(socklen_t *) &addr_l);
    inet_ntop(AF_INET, &addr.sin_addr,(char*)&loc_addr, INET_ADDRSTRLEN);
#endif

    setenv("SERVER_ADDR",(char*)&loc_addr,true);

    //TODO
    setenv("SERVER_PORT",weborf_conf.port,true);

}

/**
 * Sets env vars required by the CGI protocol
 * SERVER_SIGNATURE
 * SERVER_SOFTWARE
 * SERVER_NAME
 * GATEWAY_INTERFACE
 * REQUEST_METHOD
 * REDIRECT_STATUS (this is not specified in the CGI protocol but is needed to make php work)
 * SCRIPT_FILENAME
 * DOCUMENT_ROOT
 * REMOTE_ADDR
 * SCRIPT_NAME
 * REQUEST_URI   (It is expected that the request is like /blblabla?query and not in two separate locations, ? is expected to be replaced by \0)
 * QUERY_STRING
 * */
static inline void cgi_set_env_vars(connection_t *connection_prop) {

    //Set CGI needed vars
    setenv("SERVER_SIGNATURE",SIGNATURE,true);
    setenv("SERVER_SOFTWARE",SIGNATURE,true);
    setenv("GATEWAY_INTERFACE","CGI/1.1",true);
    setenv("REQUEST_METHOD",connection_prop->method,true); //POST GET
    setenv("SERVER_NAME", getenv("HTTP_HOST"),true); //TODO for older http version this header might not exist
    setenv("REDIRECT_STATUS","Ciao",true); // Mah.. i'll never understand php, this env var is needed
    setenv("SCRIPT_FILENAME",connection_prop->strfile,true); //This var is needed as well or php say no input file...
    setenv("DOCUMENT_ROOT",connection_prop->basedir,true);
    setenv("REMOTE_ADDR",connection_prop->ip_addr,true); //Client's address
    setenv("SCRIPT_NAME",connection_prop->page,true); //Name of the script without complete path

    //Request URI with or without a query
    if (connection_prop->get_params==NULL) {
        setenv("REQUEST_URI",connection_prop->page,true);
        setenv("QUERY_STRING","",true);//Query after ?
    } else {
        setenv("QUERY_STRING",connection_prop->get_params,true);//Query after ?

        //file and params were the same string.
        //Joining them again temporarily
        int delim=connection_prop->page_len;
        connection_prop->page[delim]='?';
        setenv("REQUEST_URI",connection_prop->page,true);
        connection_prop->page[delim]='\0';
    }

}

/**
 * Sets env vars CONTENT_LENGTH and CONTENT_TYPE
 * just copying their value from HTTP_CONTENT_LENGTH
 * and HTTP_CONTENT_TYPE
 * Hence to work the HTTP_* env variables must be
 * already set
 * */
static inline void cgi_set_env_content_length() {
    //If Content-Length field exists
    char *content_l=getenv("HTTP_CONTENT_LENGTH");
    if (content_l!=NULL) {
        setenv("CONTENT_LENGTH",content_l,true);
        setenv("CONTENT_TYPE",getenv("HTTP_CONTENT_TYPE"),true);
    }
}

/**
 * Will chdir to the same directory that contains the CGI script
 * */
static inline void cgi_child_chdir(connection_t *connection_prop) {
    //chdir to the directory
    char* last_slash=rindex(connection_prop->strfile,'/');
    last_slash[0]=0;
    chdir(connection_prop->strfile);
}

/**
 * Sets soft and hard limit for the CPU of the current
 * process.
 * the hard limit is soft_limit + 3 seconds.
 */
static inline int cgi_set_cpu_limit(int soft_limit) {
    struct rlimit limit;
    limit.rlim_cur=soft_limit;
    limit.rlim_max=soft_limit+3;

    return setrlimit(RLIMIT_CPU, &limit);
}

/**
 * TODO
 **/
static inline void cgi_dup_descriptors(int *wpipe, int *ipipe) {
    close(wpipe[0]); //Closes unused end of the pipe
    dup2(wpipe[1],1); //Redirects the stdout
    
    #ifdef HIDE_CGI_ERRORS
    close (2);
    #endif
    
    //Redirecting standard input only if there is POST data
    if (ipipe[0]!=-1) {//Send post data to script's stdin
        dup2(ipipe[0],0);
        close(ipipe[1]);
    }
    
}

/**
 * Closes unused ends of pipes, dups them,
 * sets the correct enviromental variables requested
 * by the CGI protocol and then executes the CGI.
 *
 * Will also set a CPU limit to prevent the script from
 * running forever.
 * */
static inline void cgi_execute_child(connection_t* connection_prop,char * executor) {
    environ=NULL; //Clear env vars

    cgi_set_http_env_vars(connection_prop->http_param);
    cgi_set_SERVER_ADDR_PORT(connection_prop->sock);
    cgi_set_env_vars(connection_prop);
    cgi_set_env_content_length();

    cgi_child_chdir(connection_prop);

    cgi_set_cpu_limit(SCRPT_TIMEOUT); //Sets the timeout for the script

    execl(executor,executor,(char *)0);
#ifdef SERVERDBG
    syslog(LOG_ERR,"Execution of %s failed",executor);
    perror("Execution of the page failed");
#endif
    exit(1);

}

/**
Executes a CGI script with a given interpreter and sends the resulting output
executor is the path to the binary which will execute the page
post_param contains the post data sent to the page (if present).
This can't be null, but the string pointer inside the struct can be null.
connection_prop is the struct containing all the data of the request

exec_page will fork and create pipes with the child.
The child will clean all the envvars and then set new ones as needed by CGI.


RETURNS true if header was sent TODO
*/
void cgi_exec_page(char * executor,connection_t* connection_prop) {
#ifdef SENDINGDBG
    syslog(LOG_INFO,"Executing file %s",connection_prop->strfile);
#endif

    pid_t wpid;//Child's pid
    int wpipe[] = {-1,-1};//Pipe's file descriptor
    int ipipe[] = {-1,-1};//Pipe's file descriptor, used to pass POST on script's standard input
    int results=0;

    //Pipe created and used only if there is POST data to send to the script
    if (connection_prop->post_data.len>0) {
        results+=pipe(ipipe);//Pipe to comunicate with the child
    }

    //Pipe to get the output of the child
    results+=pipe(wpipe);//Pipe to comunicate with the child

    if (results!=0 || (wpid=detached_fork())<0) { //Error, returns a no memory error
#ifdef SENDINGDBG
        syslog(LOG_CRIT,"Unable to fork to execute the file %s",connection_prop->strfile);
#endif
        close(ipipe[0]);
        close(ipipe[1]);
        close(wpipe[0]);
        close(wpipe[1]);
        
        connection_prop->response.status_code = HTTP_CODE_SERVICE_UNAVAILABLE;
        return;
    } else if (wpid==0) {
        /* never returns */
        cgi_dup_descriptors(wpipe,ipipe);
        cgi_execute_child(connection_prop,executor);
    } else { //Father: reads from pipe and sends
        connection_prop->fd_to_cgi = ipipe[1];
        connection_prop->fd_from_cgi = wpipe[0];
        connection_prop->cgi_buffer.data = calloc(HEADBUF+1,1);
        connection_prop->cgi_buffer.len = 0;
        
        http_set_chunked(connection_prop);
        
        //Closing pipes, so if they're empty read is non blocking
        close (wpipe[1]);
        close (ipipe[0]);

        if (connection_prop->post_data.len>0) {
            connection_prop->status_next = STATUS_CGI_WAIT_HEADER;
            connection_prop->status = STATUS_CGI_COPY_POST;
            connection_prop->bytes_to_copy = connection_prop->post_data.len;
        } else {
            connection_prop->status = STATUS_CGI_WAIT_HEADER;
        }
        return;
    }
    return;
}

/**
 * TODO
 **/
void cgi_wait_headers(connection_t* connection_prop) {
    //TODO this code is slow
    
    char*buffer=connection_prop->cgi_buffer.data;
    char* end = strstr(connection_prop->cgi_buffer.data,"\r\n\r\n");
    
    if (end==NULL) {
        if (HEADBUF-connection_prop->cgi_buffer.len==0)
            goto internal_error;
        
        ssize_t r = read(connection_prop->fd_from_cgi,connection_prop->cgi_buffer.data+connection_prop->cgi_buffer.len,HEADBUF-connection_prop->cgi_buffer.len);
        if (r>=0) {
            connection_prop->cgi_buffer.len+=r;
            return;
        } else 
            goto internal_error;
    }
    
    end[0]=0;
    http_append_header_safe(connection_prop,connection_prop->cgi_buffer.data);
    
    //Reading if there is a status header
    char*s=strstr(buffer,"Status: ");
    if (s!=NULL) {
                connection_prop->response.status_code=(unsigned int)strtoul( s+strlen("Status: ") , NULL, 0 );
            } else {
                connection_prop->response.status_code= HTTP_CODE_OK; //Standard status
            }

            
                //move the leftover data to the beginning of the buffer
    memmove(connection_prop->cgi_buffer.data,end+4,connection_prop->cgi_buffer.len- (connection_prop->cgi_buffer.data-end+4)  );
            
    connection_prop->status = STATUS_SEND_HEADERS;
    connection_prop->status_next = STATUS_CGI_FLUSH_HEADER_BUFFER;
    
    
     
   return;
internal_error:
    connection_prop->response.status_code = HTTP_CODE_INTERNAL_SERVER_ERROR;
            connection_prop->status = STATUS_ERR;
            return;
    
    
}

/**
 * TODO
 **/
void cgi_free_resources(connection_t* connection_prop) {
    close(connection_prop->fd_from_cgi);
    close(connection_prop->fd_to_cgi);
    free(connection_prop->cgi_buffer.data);
}