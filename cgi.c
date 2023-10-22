/*
Weborf
Copyright (C) 2010-2023  Salvo "LtWorf" Tomaselli

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
#include <strings.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <errno.h>

#include "mystring.h"
#include "cgi.h"
#include "types.h"
#include "instance.h"
#include "myio.h"

#define STDIN 0
#define STDOUT 1
#define STDERR 2

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
static inline void cgi_set_env_vars(connection_t *connection_prop,char *real_basedir) {

    //Set CGI needed vars
    setenv("SERVER_SIGNATURE",SIGNATURE,true);
    setenv("SERVER_SOFTWARE",SIGNATURE,true);
    setenv("GATEWAY_INTERFACE","CGI/1.1",true);
    setenv("REQUEST_METHOD",connection_prop->method,true); //POST GET
    setenv("REDIRECT_STATUS","Ciao",true); // Mah.. i'll never understand php, this env var is needed
    setenv("SCRIPT_FILENAME",connection_prop->strfile,true); //This var is needed as well or php say no input file...
    setenv("DOCUMENT_ROOT",real_basedir,true);
    setenv("REMOTE_ADDR",connection_prop->ip_addr,true); //Client's address
    setenv("SCRIPT_NAME",connection_prop->page,true); //Name of the script without complete path

    char* http_host = getenv("HTTP_HOST");
    if (http_host)
        setenv("SERVER_NAME", http_host,true); //TODO for older http version this header might not exist

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
    char *content_l = getenv("HTTP_CONTENT_LENGTH");
    char *content_type = getenv("HTTP_CONTENT_TYPE");

    if (content_l != NULL && content_type != NULL) {
        setenv("CONTENT_LENGTH", content_l, true);
        setenv("CONTENT_TYPE", content_type, true);
    }
}

/**
 * Will chdir to the same directory that contains the CGI script
 * */
static inline char* cgi_child_chdir(connection_t *connection_prop) {
    //chdir to the directory
    char* last_slash = rindex(connection_prop->strfile, '/');
    last_slash[0] = 0;
    chdir(connection_prop->strfile);
    last_slash[0] = '/';
    return last_slash + 1;
}

/**
 * Closes unused ends of pipes, dups them,
 * sets the correct enviromental variables requested
 * by the CGI protocol and then executes the CGI.
 *
 * Will also set an alarm to try to prevent the script from
 * running forever.
 * */
static inline void cgi_execute_child(connection_t* connection_prop,string_t* post_param,char * executor,char* real_basedir,int *wpipe,int *ipipe) {
    char* errormsg = NULL;
    close(wpipe[0]); //Closes unused end of the pipe

    close(STDOUT);
    if (dup(wpipe[1]) == -1) { //Redirects the stdout
        errormsg = "dup() failed";
        goto error;
    }

#ifdef HIDE_CGI_ERRORS
    close(STDERR);
#endif
    //Redirecting standard input only if there is POST data
    if (post_param->data != NULL) {//Send post data to script's stdin
        close(STDIN);
        if (dup(ipipe[0]) == -1) {
            errormsg = "dup() failed";
            goto error;
        }
    }

    environ = NULL; //Clear env vars

    if (strlen(executor) == 0) {
        executor = connection_prop->strfile;
    }

    cgi_set_http_env_vars(connection_prop->http_param);
    cgi_set_SERVER_ADDR_PORT(myio_getfd(connection_prop->sock));
    cgi_set_env_vars(connection_prop, real_basedir);
    cgi_set_env_content_length();
    char *filename = cgi_child_chdir(connection_prop);

    alarm(SCRPT_TIMEOUT); //Sets the timeout for the script
    execl(executor, executor, filename, (char *)0);
#ifdef SERVERDBG
    syslog(LOG_ERR,"Execution of %s failed", executor);
    perror("Execution of the page failed");
#endif
error:
#ifdef SERVERDBG
    if (errormsg)
        syslog(LOG_ERR, "%s", errormsg);
#endif
    exit(1);

}


static inline int cgi_waitfor_child(connection_t* connection_prop,string_t* post_param,char * executor,pid_t wpid,int *wpipe,int *ipipe) {
    //Closing pipes, so if they're empty read is non blocking
    close (wpipe[1]);

    //Large buffer, must contain the output of the script
    char* header_buf=malloc(MAXSCRIPTOUT+HEADBUF);

    if (header_buf==NULL) { //Was unable to allocate the buffer
        int state;
#ifdef SERVERDBG
        syslog(LOG_CRIT,"Not enough memory to allocate buffers for CGI");
#endif
        close (wpipe[0]);
        if (post_param->data!=NULL) {//Pipe created and used only if there is data to send to the script
            close (ipipe[0]); //Closes unused end of the pipe
            close (ipipe[1]); //Closes the pipe
        }
        kill(wpid,SIGKILL); //Kills cgi process
        waitpid (wpid,&state,0); //Removes zombie process
        return ERR_NOMEM;//Returns if buffer was not allocated
    }

    if (post_param->data!=NULL) {//Pipe created and used only if there is data to send to the script
        //Send post data to script's stdin
        write(ipipe[1],post_param->data,post_param->len);
        close (ipipe[0]); //Closes unused end of the pipe
        close (ipipe[1]); //Closes the pipe
    }

    //Reads output of the script
    int e_reads=read(wpipe[0],header_buf,MAXSCRIPTOUT+HEADBUF-1);
    header_buf[e_reads] = '\0';

    //Separating header from contents
    char* scrpt_buf=strstr(header_buf, "\r\n\r\n");

    long long unsigned int cgi_content_s = 0;
    if (scrpt_buf!=NULL) {
        scrpt_buf+=2;
        scrpt_buf[0]=0;
        scrpt_buf=scrpt_buf+2;
        cgi_content_s = e_reads - (scrpt_buf - header_buf);//Len of the content
    } else {//Something went wrong, ignoring the output (it's missing the headers)
        e_reads=0;
    }

    if (e_reads>0) {//There is output from script
        unsigned int status = 200; //Standard status
        {
            //Reading if there is another status
            char*s=strstr(header_buf,"Status: ");
            if (s!=NULL) {
                status=(unsigned int)strtoul( s+8 , NULL, 0 );
            }
        }

        /* There could be other data, which didn't fit in the buffer,
        so we set reads to -1 (this will make connection non-keep-alive)
        and we continue reading and writing to the socket */

        connection_prop->keep_alive = false;
        send_http_header(status, NULL, header_buf, true, -1, connection_prop);

        if (cgi_content_s) {//Sends the page if there is something to send
            myio_write(connection_prop->sock, scrpt_buf, cgi_content_s);
        }


        e_reads = 1;
        while (e_reads) {
            e_reads = read(wpipe[0],header_buf,MAXSCRIPTOUT+HEADBUF);
            if (myio_write(connection_prop->sock, header_buf, e_reads) != e_reads)
                break; //Write error, just break, can't send errors now
        }

        //Closing pipe
        close(wpipe[0]);

    } else {//No output from script, maybe terminated...
        send_err(connection_prop,500,"Internal server error");
    }

    free(header_buf);

    {
        int state;
        waitpid (wpid,&state,0); //Wait the termination of the script
    }

    return 0;
}

/**
Executes a CGI script with a given interpreter and sends the resulting output
executor is the path to the binary which will execute the page
post_param contains the post data sent to the page (if present). This can't be null, but the string pointer inside the struct can be null.
real_basedir is the basedir (according to the virtualhost)
connection_prop is the struct containing all the data of the request

exec_page will fork and create pipes with the child.
The child will clean all the envvars and then set new ones as needed by CGI.
Then the child will call alarm to set the timeout to its execution, and then will exec the script.

*/
int exec_page(char * executor,string_t* post_param,char* real_basedir,connection_t* connection_prop) {
#ifdef SENDINGDBG
    syslog(LOG_INFO,"Executing file %s",connection_prop->strfile);
#endif

    int wpid;//Child's pid
    int wpipe[2];//Pipe's file descriptor
    int ipipe[2];//Pipe's file descriptor, used to pass POST on script's standard input

    //Pipe created and used only if there is POST data to send to the script
    if (post_param->data!=NULL) {
        if (pipe(ipipe) == -1 ) {
            syslog(LOG_ERR, "Unable to create pipe");
            return ERR_NOMEM;
        }
    }

    //Pipe to get the output of the child
    if (pipe(wpipe) == -1) {
        syslog(LOG_ERR, "Unable to create pipe");
        if (post_param->data!=NULL) {
            close(ipipe[0]);
            close(ipipe[1]);
        }
        return ERR_NOMEM;
    }

    if ((wpid=fork())<0) { //Error, returns a no memory error
#ifdef SENDINGDBG
        syslog(LOG_CRIT,"Unable to fork to execute the file %s",connection_prop->strfile);
#endif
        if (post_param->data!=NULL) {
            close(ipipe[0]);
            close(ipipe[1]);
        }
        close(wpipe[1]);
        close(wpipe[0]);
        return ERR_NOMEM;
    } else if (wpid==0) {
        /* never returns */
        cgi_execute_child(connection_prop,post_param,executor,real_basedir,wpipe,ipipe);
    } else { //Father: reads from pipe and sends
        return cgi_waitfor_child(connection_prop,post_param,executor,wpid,wpipe,ipipe);
    }
    return 0;
}
