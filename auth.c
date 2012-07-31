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

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <syslog.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include "auth.h"
#include "base64.h"
#include "instance.h"
#include "types.h"

extern weborf_configuration_t weborf_conf;

#include "embedded_auth.h"

static int get_username_password(connection_t *connection_prop,char *username, char **password);

/**
Checks that the authentication socket exists and is a unix socket
*/
void auth_set_socket(char *u_socket) {
#ifdef EMBEDDED_AUTH
    weborf_conf.authsock = "embedded";
#else
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
#endif
}


/**
 * This function checks if the authentication can be granted or not calling the
 * external program.
 *
 * If no external authentication program is provided, authorizes all GET, POST
 * and OPTIONS and denies the rest.
 *
 * Returns 0 if authorization is granted.
 **/
int auth_check_request(connection_t *connection_prop) {
    if (weborf_conf.authsock==NULL) {
        switch (connection_prop->request.method_id) {
            case GET:
            case POST:
            case OPTIONS:
                return 0;
            default:
                return -1;
        }
    }

    char username[PWDLIMIT*2];
    char *password=username; //will be changed if there is a password

    if (get_username_password(connection_prop,username,&password)!=0)
        return -1;

    int result=-1;

#ifdef EMBEDDED_AUTH
    result=c_auth(connection_prop->page,
                  connection_prop->ip_addr,
                  connection_prop->method,
                  username,
                  password,
                  connection_prop->http_param);
#else
    int s,len;
    FILE *f;

    struct sockaddr_un remote;
    s=socket(AF_UNIX,SOCK_STREAM,0);

    remote.sun_family = AF_UNIX;
    strcpy(remote.sun_path, weborf_conf.authsock);
    len = strlen(remote.sun_path) + sizeof(remote.sun_family);
    if (connect(s, (struct sockaddr *)&remote, len) == -1) {//Unable to connect
        return -1;
    }

    if ((f=fdopen(s,"r+"))==NULL) {
        return -1;
    }

    fprintf(f,
            "%s\r\n%s\r\n%s\r\n%s\r\n%s\r\n%s\r\n",
            connection_prop->page,
            connection_prop->ip_addr,
            connection_prop->method,
            username,
            password,
            connection_prop->http_param);

    char b;
    if (fread(&b,1,1,f)==0) {//All data written and no output, ok
        result=0;
    }

    fclose(f);

#endif

    return result;
}

/**
This function returns username and password from an HTTP request
It has weird parameters and needs to be called like this:

    char username[PWDLIMIT*2];
    char* password=0;

    get_username_password(connection_prop,username,&password)

the username array will be filled with username and password
and the password pointer will point to the password

If there is no authentication, it will set username[0]=0 and will
not change the value of password.

return value: 0 if everything is ok

NOTE: It always assumes size of the array as PWDLIMIT*2
*/
static int get_username_password(connection_t *connection_prop,char *username, char **password) {

    char* auth=strstr(connection_prop->http_param,"Authorization: Basic ");//Locates the auth information
    if (auth==NULL) { //No auth informations
        username[0]=0;
        //password[0]=0;
        return 0;
    }

    //Retrieves provided username and password
    char*auth_end=strstr(auth,"\r\n");//Finds the end of the header
    if (auth_end==NULL) return -1;
    char a[PWDLIMIT*2];
    auth+=strlen("Authorization: Basic ");//Moves the begin of the string
    if ((auth_end-auth+1)<(PWDLIMIT*2))
        memcpy(&a,auth,auth_end-auth); //Copies the base64 encoded string to a temp buffer
    else { //Auth string is too long for the buffer
#ifdef SERVERDBG
        syslog(LOG_ERR,"Unable to accept authentication, buffer is too small");
#endif
        return HTTP_CODE_SERVICE_UNAVAILABLE;
    }

    a[auth_end-auth]=0;
    decode64(username,a);//Decodes the base64 string

    (*password)=strstr(username,":");//Locates the separator :
    if ((*password)==NULL) return -1;
    (*password[0])=0;//Nulls the separator to split username and password
    (*password)++;//make password point to the beginning of the password

    return 0;

}
