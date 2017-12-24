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
#include <fcntl.h>

#include "types.h"
#include "mynet.h"

extern weborf_configuration_t weborf_conf;


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
        dprintf(2, "If you don't have IPv6 support in kernel, try recompiling weborf, removing the line '#define IPV6' from options.h\n");
#endif
        return -1;
    }

    int flags = fcntl(s, F_GETFL, 0);
    flags |= O_NONBLOCK | O_CLOEXEC;
    fcntl(s, F_SETFL, flags);

    return s;
}

#ifdef IPV6
/**
 * Accepts an ip address.
 *
 * Returns a new buffer that needs to be freed by the
 * caller of this function.
 *
 * If addr is an ipv6 address, it is just copied into
 * the returned buffer.
 *
 * If addr is an ipv4 address, the string "::ffff:" is
 * prepended to it, turning it into an ipv6 mapped address.
 **/
char* net_map_ipv4(char* addr) {
    size_t len = strlen(addr);
    char *r;
    if (strstr(addr, ":")) {
        r = malloc(len + 1);
        memcpy(r, addr, len + 1);
    } else {
        char * prefix = "::ffff:";
        r = malloc(len + strlen(prefix) + 1);
        memcpy(r, prefix, strlen(prefix));
        memcpy(r + strlen(prefix), addr, len + 1);
    }
    return r;
}
#endif


void net_bind_and_listen(int s) {

    // Check port number
    unsigned int port = strtol(weborf_conf.port, NULL, 0);
    if (port < 1 || port > 65535) {
        dprintf(2, "Invalid port number: %u\n", port);
        exit(4);
    }

    // Create socket and set port number
#ifdef IPV6
    struct sockaddr_in6 locAddr;   //Local and remote address
    memset(&locAddr, 0, sizeof(locAddr));
    locAddr.sin6_family = AF_INET6;
    locAddr.sin6_port = htons(port);
#else
    struct sockaddr_in locAddr;
    int ipAddrL = sizeof(struct sockaddr_in);
    locAddr.sin_family = AF_INET;
    locAddr.sin_port = htons(port);
#endif

    // Set IP address
    if (weborf_conf.ip) {
#ifdef IPV6
        char * ip = net_map_ipv4(weborf_conf.ip);
        int rpt = inet_pton(AF_INET6, ip, &locAddr.sin6_addr);
        free(ip);
#else
        int rpt = inet_pton(AF_INET, weborf_conf.ip, &locAddr.sin_addr);
#endif
        if (rpt == 0) {
            dprintf(2, "Invalid IPv%c address: %s\n", IPVERSION, weborf_conf.ip);
            exit(2);
        }
    } else {
        weborf_conf.ip = "any";
#ifdef IPV6
        locAddr.sin6_addr = in6addr_any;
#else
        inet_pton(AF_INET, "0.0.0.0", &locAddr.sin_addr);
#endif
    }

    // Log listened interface
    syslog(LOG_INFO, "Listening on address: %s:%u",
           weborf_conf.ip, port);

    // Bind
    if (bind(s, (struct sockaddr *) &locAddr, sizeof(locAddr)) < 0) {
        perror("trying to bind");
        syslog(LOG_ERR, "Port %u already in use", port);
        exit(3);
    }
    listen(s, MAXQ); //Listen to the socket
}


void net_getpeername(int socket,char* buffer) {

#ifdef IPV6

    struct sockaddr_storage t_addr;
    socklen_t addr_l=sizeof(t_addr);

    getpeername(socket, (struct sockaddr *)&t_addr, &addr_l);

    if (t_addr.ss_family==AF_INET) {
        struct sockaddr_in *addr =(struct sockaddr_in *)&t_addr;
        char temp_buffer[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(addr->sin_addr), temp_buffer, INET_ADDRSTRLEN);
        snprintf(buffer,INET6_ADDRSTRLEN,"::ffff:%s",temp_buffer);
    } else {
        struct sockaddr_in6 *addr =(struct sockaddr_in6 *)&t_addr;
        inet_ntop(AF_INET6, &(addr->sin6_addr), buffer, INET6_ADDRSTRLEN);
    }
#else
    struct sockaddr_in addr;
    socklen_t addr_l=sizeof(struct sockaddr_in);

    getpeername(socket, (struct sockaddr *)&addr,&addr_l);
    inet_ntop(AF_INET, &addr.sin_addr, buffer, INET_ADDRSTRLEN);
#endif
}
