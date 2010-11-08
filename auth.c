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
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <syslog.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include "options.h"
#include "types.h"
#include "auth.h"
#include "instance.h"
#include "base64.h"

extern weborf_configuration_t weborf_conf;
/**
Checks that the authentication socket exists and is a unix socket
*/
void auth_set_socket(char *u_socket) {
    struct stat sb;
    if (stat(u_socket, &sb) == -1) {
        perror("Existing unix socket expected");
#ifdef SERVERDBG
        syslog(LOG_ERR, "%s doesn't exist", u_socket);
#endif
        exit(5);
    }

    if ((sb.st_mode & S_IFMT) != S_IFSOCK) {
#ifdef SERVERDBG
        syslog(LOG_ERR, "%s is not a socket", u_socket);
#endif
        write(2,"Socket expected\n",16);
        exit(5);
    }


    weborf_conf.authsock = u_socket;
}

/**
This function checks if the authentication can be granted or not calling the external program.
Returns 0 if authorization is granted.
*/
int auth_check_request(connection_t *connection_prop) {
    if (weborf_conf.authsock==NULL) return 0;

    char username[PWDLIMIT*2];
    char* password=username; //will be changed if there is a password

    char* auth=strstr(connection_prop->http_param,"Authorization: Basic ");//Locates the auth information
    if (auth==NULL) { //No auth informations
        username[0]=0;
        //password[0]=0;
    } else { //Retrieves provided username and password
        char*auth_end=strstr(auth,"\r\n");//Finds the end of the header
        if (auth_end==NULL) return -1;
        char a[PWDLIMIT*2];
        auth+=21;//Moves the begin of the string to exclude Authorization: Basic
        if ((auth_end-auth+1)<(PWDLIMIT*2))
            memcpy(&a,auth,auth_end-auth); //Copies the base64 encoded string to a temp buffer
        else { //Auth string is too long for the buffer
#ifdef SERVERDBG
            syslog(LOG_ERR,"Unable to accept authentication, buffer is too small");
#endif
            return ERR_NOMEM;
        }

        a[auth_end-auth]=0;
        decode64(username,a);//Decodes the base64 string

        password=strstr(username,":");//Locates the separator :
        if (password==NULL) return -1;
        password[0]=0;//Nulls the separator to split username and password
        password++;//make password point to the beginning of the password
    }

    short int result=-1;
    {
        int s,len;
        struct sockaddr_un remote;
        s=socket(AF_UNIX,SOCK_STREAM,0);

        remote.sun_family = AF_UNIX;
        strcpy(remote.sun_path, weborf_conf.authsock);
        len = strlen(remote.sun_path) + sizeof(remote.sun_family);
        if (connect(s, (struct sockaddr *)&remote, len) == -1) {//Unable to connect
            return -1;
        }
        char* auth_str=malloc(HEADBUF+PWDLIMIT*2);
        if (auth_str==NULL) {
#ifdef SERVERDBG
            syslog(LOG_CRIT,"Not enough memory to allocate buffers");
#endif
            return -1;
        }

        int auth_str_l=snprintf(auth_str,HEADBUF+PWDLIMIT*2,"%s\r\n%s\r\n%s\r\n%s\r\n%s\r\n%s\r\n",connection_prop->page,connection_prop->ip_addr,connection_prop->method,username,password,connection_prop->http_param);
        if (write(s,auth_str,auth_str_l)==auth_str_l && read(s,auth_str,1)==0) {//All data written and no output, ok
            result=0;
        }

        close(s);
        free(auth_str);
    }

    return result;
}
