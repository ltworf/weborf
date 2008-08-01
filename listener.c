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
@author Salvo Rinaldi <salvin@bluebottle.com>
*/

#include "listener.h"
#include "instance.h"
#include "queue.h"
#include "options.h"
#include "utils.h"
#define _GNU_SOURCE
#include <getopt.h>

syn_queue_t queue; //Queue for opened sockets


pthread_mutex_t m_free;//Mutex to modify t_free
unsigned int t_free=0;//Free threads

pthread_mutex_t m_thread_c;//Mutex to modify thread_c
unsigned int thread_c=0;//Number of threads

char * basedir=BASEDIR;//Base directory
char* authbin;//Executable that will authenticate

uid_t uid=ROOTUID;//Uid to use after bind

pthread_attr_t t_attr;//thread's attributes


/**
Increases or decreases the number of current active thread.
This function is thread safe.

Notice that this will not start or stop threads, just change the value of thread_c.
*/
void chn_thread_count(unsigned int val) {

    pthread_mutex_lock(&m_thread_c);
    thread_c+=val;
    pthread_mutex_unlock(&m_thread_c);
}

/**
Sets t_attr to make detached threads
*/
void init_thread_attr() {
    int rc = pthread_attr_init(&t_attr);
    rc = pthread_attr_setdetachstate(&t_attr, PTHREAD_CREATE_DETACHED);
}

/**
Starts threads
Specify how many threads start.
*/
void init_threads(unsigned int count) {
    static long int id=1;
    //t_free=MAXTHREAD;
    int i;

    pthread_t t_id;//Unused var, thread's system id

    pthread_mutex_lock(&m_free);

    for (i = 1; i <= count; i++) {
        pthread_create(&t_id, &t_attr, instance, (void*)(id++));

    }
    chn_thread_count(count);//Increases the counter of active threads
    t_free+=count;
#ifdef THREADDBG
    syslog(LOG_DEBUG,"There are %d free threads",t_free);
#endif
    pthread_mutex_unlock(&m_free);
}

/**
This function inits the logger.
Will use syslogd
*/
void init_logger() {
    openlog("weborf",LOG_ODELAY,LOG_DAEMON);
#ifdef SERVERDBG
    syslog(LOG_INFO,"Startig server...");
#endif
}

int main(int argc, char * argv[]) {

    init_logger();//Inits the logger

    int s, s1;//Socket

    char *ip=NULL;//IP addr with default value
    char *port=PORT;//port with default value

#ifdef IPV6
    struct sockaddr_in6 locAddr, farAddr;//Local and remote address
    socklen_t farAddrL, ipAddrL;
#else
    struct sockaddr_in locAddr,farAddr;
    int farAddrL, ipAddrL;
#endif


    while (1) { //Block to read command line

        //Declares options
        static struct option long_options[] = {
            {"version", no_argument, 0, 'v'},
            {"help", no_argument, 0, 'h'},
            {"port", required_argument, 0, 'p'},
            {"ip", required_argument, 0, 'i'},
            {"uid", required_argument, 0, 'u'},
            {"daemonize", no_argument, 0, 'd'},
            {"basedir", required_argument, 0, 'b'},
            {"auth", required_argument, 0, 'a'},
            {"moo", no_argument, 0, 'm'},
            {0, 0, 0, 0}
        };
        static int c;//Identify the readed option
        int option_index=0;

        //Reading one option and telling what options are allowed and what needs an argument
        c = getopt_long (argc, argv, "mvhp:i:u:db:a:",long_options, &option_index);

        //If there are no options it continues
        if (c == -1)
            break;

        switch (c) {
        case 'b'://Basedirectory
            setBasedir(optarg);
            break;
        case 'v'://Show version and exit
            version();
            break;
        case 'h'://Show help and exit
            help();
            break;
        case 'p'://Set port
            port=optarg;
            break;
        case 'i'://Set ip address
            ip=optarg;
            break;
        case 'u'://Set uid
            uid=strtol( optarg , NULL, 0 );
            break;
        case 'd'://Daemonize
            if (fork()==0)
                signal(SIGHUP, SIG_IGN);
            else
                exit(0);
            break;
        case 'a'://Set authentication script
            setAuthbin(optarg);
            break;
        case 'm'://Supercow!
            moo();
            break;
        default:
            exit(19);
        }

    }


    printf("Weborf\n");
    printf("This program comes with ABSOLUTELY NO WARRANTY.\n");
    printf("This is free software, and you are welcome to redistribute it\n");
    printf("under certain conditions.\nFor details see the GPLv3 Licese.\n");
    printf("Run %s --help to see the options\n",argv[0]);

    //Creates the socket
#ifdef IPV6
    s = socket(PF_INET6, SOCK_STREAM, 0);
#else
    s = socket(PF_INET, SOCK_STREAM, 0);
#endif

    int val=1;
    //Makes port reusable immediately after termination.
    if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)) < 0) {
        perror("ruseaddr(any)");
        char*suggestion="If you don't have any IPv6 address, try recompiling weborf, removing the line '#define IPV6' from options.h\n";
        write(2,suggestion,strlen(suggestion));
        return 1;
    }

#ifdef IPV6
    ipAddrL = farAddrL = sizeof(struct sockaddr_in);
    //Bind
    memset(&locAddr, 0, sizeof(locAddr));
    locAddr.sin6_family = AF_INET6;
    locAddr.sin6_port   = htons(strtol( port , NULL, 0 ));
    if (ip==NULL) {//Default ip, listens to all the interfaces
        locAddr.sin6_addr   = in6addr_any;
    } else {//Custom ip
        if (inet_pton(AF_INET6,ip,&locAddr.sin6_addr)==0) {
            printf("Invalid IP address: %s\n",ip);
            exit(2);
        }
    }


    if ( bind(s, (struct sockaddr *) &locAddr, sizeof(locAddr)) <0 ) {
        perror("trying to bind");
#ifdef SOCKETDBG
        syslog(LOG_ERR,"Port %d already in use",ntohs(locAddr.sin6_port));
#endif
        exit(-1);
    }

#else
    //Prepares socket's address
    locAddr.sin_family = AF_INET;//Internet socket

    {//Check the validity of port param and uses it
        unsigned int p=strtol( port , NULL, 0 );
        if (p<1 || p>65535) {
            printf("Invalid port number: %d\n",p);
            exit(2);
        }
        locAddr.sin_port=htons(p);
    }

    if (ip==NULL) ip="0.0.0.0";//Default ip address
    if (inet_aton(ip, &locAddr.sin_addr)==0) {//Converts ip to listen in binary format
        printf("Invalid IP address: %s\n",ip);
        exit(2);
    }

#ifdef SOCKETDBG
    syslog(LOG_INFO,"Listening on address: %s:%d", inet_ntoa(locAddr.sin_addr),ntohs(locAddr.sin_port));
#endif


    ipAddrL = farAddrL = sizeof(struct sockaddr_in);


    //Bind
    if ( bind(s, (struct sockaddr *) &locAddr, ipAddrL) == -1 ) {
        perror("trying to bind");
#ifdef SOCKETDBG
        syslog(LOG_ERR,"Port %d already in use",ntohs(locAddr.sin_port));
#endif
        exit(-1);
    }

#endif




    //Changes UID.
    if (uid!=ROOTUID) {
        if (setuid(uid)==0) {
            //Uid changed correctly
#ifdef SERVERDBG
            syslog(LOG_INFO,"Changed uid. New one is %d",uid);
#endif
        } else {
            //Not enough permissions i guess...
#ifdef SERVERDBG
            syslog(LOG_ERR,"Unable to change uid.");
#endif
            perror("Unable to change uid");
            exit(9);
        }
    }

    //init the queue for opened sockets
    q_init(&queue,MAXTHREAD+1);

    //Starts the 1st group of threads
    init_thread_attr();
    init_threads(INITIALTHREAD);

    {//Starts the monitoring thread, to close unused threads
        pthread_t t_id;//Unused var
        pthread_create(&t_id, NULL, t_shape, (void*)NULL);
    }

    //Handle SIGINT and SIGTERM
    signal(SIGINT, quit);
    signal(SIGTERM, quit);


    listen(s, MAXQ);//Listen to the socket

    int loc_free=t_free;//Local t_free, used to avoid deadlock

    //Infinite cycle, accept connections
#ifdef IPV6
    while ((s1 = accept(s, (struct sockaddr *) &farAddr, &farAddrL)) != -1) {

        char* ip_addr=malloc(INET6_ADDRSTRLEN);//Buffer for IP Address, to give to the thread
        if (ip_addr!=NULL ) {
            getpeername(s1, (struct sockaddr *)&farAddr, &farAddrL);
            inet_ntop(AF_INET6, &farAddr.sin6_addr, ip_addr, INET6_ADDRSTRLEN);
        }
#else
    while ((s1 = accept(s, (struct sockaddr *) &farAddr,(socklen_t *)&farAddrL)) != -1) {

        char* ip_addr=malloc(INET6_ADDRSTRLEN);
        { //Buffer for ascii IP addr, will be freed by the thread
            char* ip=inet_ntoa(farAddr.sin_addr);
            memcpy(ip_addr,ip,strlen(ip)+1);
        }

#endif


#ifdef SOCKETDBG
        syslog(LOG_INFO,"Connection from %s", ip_addr);
#endif

        if (s1>=0  && loc_free>0) { //Adds s1 to the queue
            q_put(&queue, s1,ip_addr);
        } else { //Closes the socket if there aren't enough free threads.
#ifdef REQUESTDBG
            syslog(LOG_ERR,"Not enough resources, dropping connection...");
#endif
            close(s1);
        }

        //Obtains the number of free threads
        pthread_mutex_lock(&m_free);
        loc_free=t_free;
        pthread_mutex_unlock(&m_free);

        //Start new thread if needed
        if (loc_free<=LOWTHREAD) { //Need to start new thread
            if (thread_c+INITIALTHREAD<MAXTHREAD) {//Starts a group of threads
                init_threads(INITIALTHREAD);
            } else { //Can't start a group because the limit is close, starting less than a whole group
                init_threads(MAXTHREAD-thread_c);
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
    syslog(LOG_INFO,"Stopping server...");
#endif

    closelog();
    exit(0);
}

/**
Sets the base dir, making sure that it is really a directory.
 */
void setBasedir(char * bd) {
    int f_mode=fileIsA(bd);//Gets file's mode
    if (!S_ISDIR(f_mode)) {
        //Not a directory
        printf("%s must be a directory\n",bd);
        exit(1);
    }
    basedir=bd;
}

/**
Checks that the authentication executable exists and is executable
*/
void setAuthbin(char* bin) {
    char command[600];
    sprintf(command,"test -x %s",bin);
    if (system(command)!=0) { //Doesn't exist or it isn't executable
        printf("%s doesn't exist or it is not executable\n",bin);
        exit(1);
    }
    authbin=bin;
}


/**
This function, executed as a thread, terminates threads if there are too much free threads.

It works polling the number of free threads and writing an order of termination if too much of them are free.

Policies of this function (polling frequence and limit for free threads) are defined in options.h
 */
void* t_shape(void * nulla) {

    int loc_free;

    for (;;) {
        sleep(THREADCONTROL);

        //Reads the number of free threads
        pthread_mutex_lock(&m_free);
        loc_free=t_free;
        pthread_mutex_unlock(&m_free);

        if (loc_free>MAXFREETHREAD) { //Too much free threads, terminates one of them
            q_put(&queue, -1,0);//Write the termination order to the queue, the thread who will read it, will terminate
            chn_thread_count(-1);//Decreases the number of free total threads
        }
    }
    return NULL; //make gcc happy
}
