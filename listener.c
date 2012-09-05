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
#include <netinet/in.h>
#include <arpa/inet.h>
#include <locale.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <syslog.h>
#include <getopt.h>
#include <pthread.h>


#include "listener.h"
#include "instance.h"
#include "queue.h"
#include "utils.h"
#include "mystring.h"
#include "types.h"
#include "cachedir.h"
#include "configuration.h"
#include "mynet.h"

#define _GNU_SOURCE

syn_queue_t queue;              //Queue for opened sockets
extern weborf_configuration_t weborf_conf;
pthread_attr_t t_attr;          //thread's attributes
pthread_key_t thread_key;            //key for pthread_setspecific

RETSIGTYPE quit();
RETSIGTYPE print_queue_status();


/**
Sets t_attr to make detached threads
and initializes pthread_keys
*/
static void init_thread_attr() {
    pthread_attr_init(&t_attr);
    pthread_attr_setdetachstate(&t_attr, PTHREAD_CREATE_DETACHED);
    pthread_key_create(&thread_key,NULL);

}

/**
Starts one thread
*/
void init_threads(unsigned int count) {
    pthread_t t_id;//Unused var, thread's system id
    pthread_create(&t_id, &t_attr, instance, (void *)NULL);
}

/**
This function inits the logger.
Will use syslogd
*/
static void init_logger() {
    openlog(NAME, LOG_ODELAY, LOG_DAEMON);
}

/**
 * Set quit action on SIGTERM and SIGINT
 * and prints the internal status on SIGUSR1
 * */
static void init_signals() {
    signal(SIGINT, quit);
    signal(SIGTERM, quit);
    signal(SIGPIPE, SIG_IGN);

    //Prints queue status with this signal
    signal(SIGUSR1, print_queue_status);
}

int main(int argc, char *argv[]) {
    int s, s1;          //Socket descriptors

#ifdef HAVE_SETLOCALE
    setlocale(LC_CTYPE, "C");
#endif



    configuration_load(argc,argv);

    init_signals();
    init_logger();

    if (weborf_conf.is_inetd) inetd();

    print_start_disclaimer(argv);

#ifdef SERVERDBG
    syslog(LOG_INFO, "Starting server...");
#endif


    weborf_conf.socket = net_create_server_socket();
    net_bind_and_listen(weborf_conf.socket);

    set_new_uid(weborf_conf.uid);

    //init the queue for opened sockets
    if (q_init(&queue, MAXTHREAD + 1) != 0)
        exit(NOMEM);

    //Starts the 1st group of threads
    init_thread_attr();
    init_threads(INITIALTHREAD);
    
    while(1) sleep(12);
    return 0; //FIXME

    //Infinite cycle, accept connections
    while (1) {
        s1 = accept(s, NULL,NULL);

        if (s1 >= 0 && q_put(&queue, s1)!=0) { //Adds s1 to the queue
#ifdef REQUESTDBG
            syslog(LOG_ERR,"Not enough resources, dropping connection...");
#endif
            close(s1);
        }

    }
    return 0;

}

/**
SIGINT and SIGTERM signal handler
*/
RETSIGTYPE quit() {
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

/**
 * Prints internal status
 **/
RETSIGTYPE print_queue_status() {
    //TODO
}