/*
Weborf
Copyright (C) 2007  Salvo "LtWorf" Tomaselli

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
@author Giuseppe Pappalardo <pappalardo@dmi.unict.it>
@author Salvo Rinaldi <salvin@anche.no>
*/
#include "options.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <syslog.h>
#include <getopt.h>
#include <pthread.h>

#include "listener.h"
#include "instance.h"
#include "utils.h"
#include "mystring.h"
#include "types.h"
#include "cachedir.h"
#include "configuration.h"
#include "mynet.h"

#define _GNU_SOURCE

t_thread_info thread_info;

extern weborf_configuration_t weborf_conf;

pthread_attr_t t_attr;          //thread's attributes

pthread_key_t thread_key;            //key for pthread_setspecific

/**
This function inits the logger.
Will use syslogd
*/
static void init_logger() {
    openlog(NAME, LOG_ODELAY, LOG_DAEMON);
#ifdef SERVERDBG
    if (!weborf_conf.is_inetd)
        syslog(LOG_INFO, "Starting server...");
#endif

}

/**
 * Set quit action on SIGTERM and SIGINT
 * and prints the internal status on SIGUSR1
 * */
static void init_signals() {
    signal(SIGINT, quit);
    signal(SIGTERM, quit);
    signal(SIGPIPE, SIG_IGN);
    signal(SIGCHLD, wait_child);
}

int main(int argc, char *argv[]) {
    int s, s1;          //Socket descriptors

    configuration_load(argc,argv);

    init_signals();
    init_logger();

    if (weborf_conf.is_inetd) inetd();

    print_start_disclaimer(argv);

    s = net_create_server_socket();
    net_bind_and_listen(s);

    set_new_uid(weborf_conf.uid);

    //Infinite cycle, accept connections
    while (1) {
        s1 = accept(s, NULL,NULL);

        if (s1 >= 0) {
            int pid=fork();
            if (pid==0) {
                close(0);
                dup(s1);
                inetd();
            } else {
                close(s1);
            }
        }

    }
    return 0;

}

/**
SIGINT and SIGTERM signal handler
*/
void quit() {
#ifdef SERVERDBG
    syslog(LOG_INFO, "Stopping server...");
#endif

    closelog();
    exit(0);
}

void set_new_uid(int uid) {
    //Changes UID.
    if (uid != ROOTUID) {
        if (setuid(uid) == 0) {
            //Uid changed correctly
#ifdef SERVERDBG
            syslog(LOG_INFO, "Changed uid. New one is %d", uid);
#endif
        } else {
            //Not enough permissions i guess...
#ifdef SERVERDBG
            syslog(LOG_ERR, "Unable to change uid.");
#endif
            perror("Unable to change uid");
            exit(9);
        }
    }
}

void wait_child() {
    int s;
    wait(&s);
}
