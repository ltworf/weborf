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

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <syslog.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdlib.h>

#include "types.h"
#include "mynet.h"

extern weborf_configuration_t weborf_conf;


/**
 * removes TCP_CORK in order to send an incomplete frame if needed
 * and sets it again to forbid incomplete frames again.
 *
 * it is Linux specific. Does nothing otherwise
 */
void net_sock_flush (int sock) {
#ifdef TCP_CORK
    int val=0;
    setsockopt(sock, IPPROTO_TCP, TCP_CORK, (char *)&val, sizeof(val));
    val=1;
    setsockopt(sock, IPPROTO_TCP, TCP_CORK, (char *)&val, sizeof(val));
#else
#warning "NO TCP_CORK"
#endif
}

/**
 * Same as the combination of getperrname and
 * inet_ntop, will save the string address of the
 * client for the socket within the buffer.
 * It expects the buffer to have the correct size.
 *
 * */
void net_getpeername(int socket,char* buffer) {

#ifdef IPV6
    struct sockaddr_in6 addr;
    socklen_t addr_l=sizeof(struct sockaddr_in6);

    getpeername(socket, (struct sockaddr *)&addr, &addr_l);
    inet_ntop(AF_INET6, &addr.sin6_addr, buffer, INET6_ADDRSTRLEN);
#else
    struct sockaddr_in addr;
    socklen_t addr_l=sizeof(struct sockaddr_in);

    getpeername(socket, (struct sockaddr *)&addr,&addr_l);
    inet_ntop(AF_INET, &addr.sin_addr, buffer, INET_ADDRSTRLEN);
#endif
}

/**
 * Creates the server socket and performs some setsockopt
 * on it.
 * */
int net_create_server_socket() {
    int s;
    int val;

//Creates the socket
#ifdef IPV6
    s = socket(PF_INET6, SOCK_STREAM, 0);
#else
    s = socket(PF_INET, SOCK_STREAM, 0);
#endif


    /*
    This futile system call is here just because a debian mantainer decided
    that the default behavior must be to listen only to IPv6 connections
    excluding IPv4 ones.
    So this restores the logic and normal behavior of accepting connections
    without being racist about the client's address.
    */

#ifdef IPV6
    val=0;
    setsockopt(s, IPPROTO_IPV6, IPV6_V6ONLY, (char *)&val, sizeof(val));
#endif

    val = 1;
    //Makes port reusable immediately after termination.
    if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)) < 0) {
        perror("ruseaddr(any)");
        syslog(LOG_ERR, "reuseaddr(any)");
#ifdef IPV6
        char *suggestion = "If you don't have IPv6 support in kernel, try recompiling weborf, removing the line '#define IPV6' from options.h\n";
        write(2, suggestion, strlen(suggestion));
#endif
        return -1;
    }

    return s;
}

/**
Binds the socket s and listens to it
The port and address used are taken from
weborf_conf
*/
void net_bind_and_listen(int s) {

#ifdef IPV6
    struct sockaddr_in6 locAddr;   //Local and remote address
    //socklen_t ipAddrL = sizeof(struct sockaddr_in6);
#else
    struct sockaddr_in locAddr;
    int ipAddrL = sizeof(struct sockaddr_in);
#endif


#ifdef IPV6

    //Bind
    memset(&locAddr, 0, sizeof(locAddr));
    locAddr.sin6_family = AF_INET6;
    locAddr.sin6_port = htons(strtol(weborf_conf.port, NULL, 0));
    if (weborf_conf.ip == NULL) { //Default ip, listens to all the interfaces
        locAddr.sin6_addr = in6addr_any;
    } else { //Custom ip
        if (inet_pton(AF_INET6, weborf_conf.ip, &locAddr.sin6_addr) == 0) {
            printf("Invalid IP address: %s\n", weborf_conf.ip);
            exit(2);
        }
    }


    if (bind(s, (struct sockaddr *) &locAddr, sizeof(locAddr)) < 0) {
        perror("trying to bind");
#ifdef SOCKETDBG
        syslog(LOG_ERR, "Port %d already in use",
               ntohs(locAddr.sin6_port));
#endif
        exit(3);
    }
#else
    //Prepares socket's address
    locAddr.sin_family = AF_INET;   //Internet socket

    {
        //Check the validity of port param and uses it
        unsigned int p = strtol(weborf_conf.port, NULL, 0);
        if (p < 1 || p > 65535) {
            printf("Invalid port number: %d\n", p);
            exit(4);
        }
        locAddr.sin_port = htons(p);
    }

    if (weborf_conf.ip == NULL)
        weborf_conf.ip = "0.0.0.0"; //Default ip address
    if (inet_aton(weborf_conf.ip, &locAddr.sin_addr) == 0) { //Converts ip to listen in binary format
        printf("Invalid IP address: %s\n", weborf_conf.ip);
        exit(2);
    }
#ifdef SOCKETDBG
    syslog(LOG_INFO, "Listening on address: %s:%d",
           inet_ntoa(locAddr.sin_addr), ntohs(locAddr.sin_port));
#endif


    //Bind
    if (bind(s, (struct sockaddr *) &locAddr, ipAddrL) == -1) {
        perror("trying to bind");
#ifdef SOCKETDBG
        syslog(LOG_ERR, "Port %d already in use", ntohs(locAddr.sin_port));
#endif
        exit(3);
    }
#endif

    listen(s, MAXQ); //Listen to the socket

}
