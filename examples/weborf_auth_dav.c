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

/*Example of authentication program

This program will allow all GET and POST methods,
will allow PROPFIND MKCOL and PUT only from 10.0.*
And will deny the rest.

For avoiding trouble, also the OPTIONS method is always
enabled
*/


#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#define SOCK_PATH "/tmp/weborf_auth.socket"
#define SIZE 1024

int main(void) {
    int s,s2, t, len;
    struct sockaddr_un local, remote;
    char str[SIZE];

    //Removes the socket if already exists
    unlink(SOCK_PATH);

    if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(1);
    }

    local.sun_family = AF_UNIX;
    strcpy(local.sun_path, SOCK_PATH);
    unlink(SOCK_PATH);
    len = strlen(local.sun_path) + sizeof(local.sun_family);

    if (bind(s, (struct sockaddr *)&local, len) == -1) {
        perror("bind");
        exit(1);
    }

    if (listen(s, 5) == -1) {
        perror("listen");
        exit(1);
    }


    //Cycle to accept connections
    for (;;) {
        t = sizeof(remote);
        if ((s2 = accept(s, (struct sockaddr *)&remote, &t)) == -1) {
            perror("accept");
            exit(1);
        }

        t = recv(s2, str, SIZE, 0);
        str[t] = '\0';


        //Parse string to get the things we need
        char *uri = strtok(str,"\r\n");
	char *ip = strtok(NULL,"\r\n");
	char *method = strtok(NULL,"\r\n");


        //Allow GET, POST, and OPTIONS
	if (strncmp(method,"GET",3)==0 || strncmp(method,"POST",4)==0 || strncmp(method,"OPTIONS",7)) {
            close(s2); //Closing the connection without sending anything will result in weborf accept the request
        }
        //Allow PROPFIND MKCOL PUT from local network
        else if (strncmp(method,"PROPFIND",8)==0 || strncmp(method,"PUT",3)==0 || strncmp(method,"MKCOL",5)==0) {
            if (strncmp(ip,"::ffff:10.0.",12)!=0)
                send(s2,"c",1,0);
            close(s2);
        }

        //Disallow the rest (DELETE, MOVE, COPY)
        else {
            send(s2,"c", 1,0);
            close(s2);
        }

    }

    return 0;
}

