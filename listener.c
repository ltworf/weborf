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

#include "listener.h"
#include "instance.h"
#include "queue.h"
#include "options.h"
#include "utils.h"
#include "mystring.h"
#include "types.h"
#define _GNU_SOURCE

syn_queue_t queue;              //Queue for opened sockets

t_thread_info thread_info;

char *basedir = BASEDIR;        //Base directory
char *authsock;                  //Executable that will authenticate
bool exec_script = true;        //Execute scripts if true, sends the file if false

uid_t uid = ROOTUID;            //Uid to use after bind

pthread_attr_t t_attr;          //thread's attributes

char *indexes[MAXINDEXCOUNT];   //List of pointers to index files
int indexes_l = 1;              //Count of the list

bool virtual_host = false;      //True if must check for virtual hosts

array_ll cgi_paths;             //Paths to cgi binaries

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
    static long int id = 1;
    //t_free=MAXTHREAD;
    int effective=0,i;

    pthread_t t_id;//Unused var, thread's system id

    pthread_mutex_lock(&thread_info.mutex);
    //Check condition within the lock
    if (thread_info.count + count < MAXTHREAD) {

        //Start
        for (i = 1; i <= count; i++)
            if (pthread_create(&t_id, &t_attr, instance, (void *) (id++))==0) effective++;

        thread_info.count+=effective; // increases the count of started threads
#ifdef THREADDBG
        syslog(LOG_DEBUG, "There are %d free threads", t_free);
#endif

#ifdef SERVERDBG
        if (effective!=count)
            syslog(LOG_CRIT,"Unable to launch the required threads");
#endif


    }
    pthread_mutex_unlock(&thread_info.mutex);
}

/**
This function inits the logger.
Will use syslogd
*/
void init_logger() {
    openlog("weborf", LOG_ODELAY, LOG_DAEMON);
#ifdef SERVERDBG
    syslog(LOG_INFO, "Starting server...");
#endif
}

int main(int argc, char *argv[]) {
    int s, s1;          //Socket

    char *ip = NULL;    //IP addr with default value
    char *port = PORT;  //port with default value

    //Init thread_info
    pthread_mutex_init(&thread_info.mutex, NULL);
    thread_info.count=0;
    thread_info.free=0;

    init_logger();      //Inits the logger


#ifdef IPV6
    struct sockaddr_in6 locAddr, farAddr;	//Local and remote address
    socklen_t farAddrL, ipAddrL;
#else
    struct sockaddr_in locAddr, farAddr;
    int farAddrL, ipAddrL;
#endif

    //default index file
    indexes[0] = INDEX;

    //default cgi
    cgi_paths.len=4;
    cgi_paths.data[0]=".php";
    cgi_paths.data[1]=CGI_PHP;
    cgi_paths.data[2]=".py";
    cgi_paths.data[3]=CGI_PY;
    cgi_paths.data_l[0]=4;
    cgi_paths.data_l[1]=strlen(CGI_PHP);
    cgi_paths.data_l[2]=3;
    cgi_paths.data_l[3]=strlen(CGI_PY);

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
            {"index", required_argument, 0, 'I'},
            {"auth", required_argument, 0, 'a'},
            {"virtual", required_argument, 0, 'V'},
            {"moo", no_argument, 0, 'm'},
            {"noexec", no_argument, 0, 'x'},
            {"cgi", required_argument, 0, 'c'},
            {0, 0, 0, 0}
        };
        static int c; //Identify the readed option
        int option_index = 0;

        //Reading one option and telling what options are allowed and what needs an argument
        c = getopt_long(argc, argv, "mvhp:i:I:u:dxb:a:V:c:", long_options,
                        &option_index);

        //If there are no options it continues
        if (c == -1)
            break;

        switch (c) {
        case 'c': { //Setting list of cgi
            int i = 0;
            cgi_paths.len = 1; //count of indexes
            cgi_paths.data[0] = optarg; //1st one points to begin of param
            while (optarg[i++] != 0) { //Reads the string
                if (optarg[i] == ',') {
                    optarg[i++] = 0; //Nulling the comma
                    //Increasing counter and making next item point to char after the comma
                    cgi_paths.data[cgi_paths.len++] = &optarg[i];
                    if (cgi_paths.len == MAXINDEXCOUNT) {
                        perror("Too much cgis, change MAXINDEXCOUNT in options.h to allow more");
                        exit(6);
                    }
                }
            }

            for (i=0; i<cgi_paths.len; i++) {
                cgi_paths.data_l[i]=strlen(cgi_paths.data[i]);
            }

        }
        break;
        case 'V': { //Setting virtual hosts
            virtual_host = true;

            int i = 0;
            char *virtual = optarg; //1st one points to begin of param

            while (optarg[i++] != 0) { //Reads the string
                if (optarg[i] == ',') {
                    optarg[i++] = 0; //Nulling the comma
                    putenv(virtual);
                    virtual = &optarg[i];

                }
            }
            putenv(virtual);
        }
        break;
        case 'I': { //Setting list of indexes
            int i = 0;
            indexes_l = 1; //count of indexes
            indexes[0] = optarg; //1st one points to begin of param
            while (optarg[i++] != 0) { //Reads the string

                if (optarg[i] == ',') {
                    optarg[i++] = 0; //Nulling the comma
                    //Increasing counter and making next item point to char after the comma
                    indexes[indexes_l++] = &optarg[i];
                    if (indexes_l == MAXINDEXCOUNT) {
                        perror("Too much indexes, change MAXINDEXCOUNT in options.h to allow more");
                        exit(6);
                    }
                }
            }

        }
        break;

        case 'b':   //Basedirectory
            setBasedir(optarg);
            break;
        case 'x':   //Noexec scripts
            exec_script = false;
            break;
        case 'v':   //Show version and exit
            version();
            break;
        case 'h':   //Show help and exit
            help();
            break;
        case 'p':   //Set port
            port = optarg;
            break;
        case 'i':   //Set ip address
            ip = optarg;
            break;
        case 'u':   //Set uid
            uid = strtol(optarg, NULL, 0);
            break;
        case 'd':   //Daemonize
            if (fork() == 0)
                signal(SIGHUP, SIG_IGN);
            else
                exit(0);
            break;
        case 'a':   //Set authentication script
            set_authsocket(optarg);
            break;
        case 'm':   //Supercow!
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
    printf("Run %s --help to see the options\n", argv[0]);

    setenv("SERVER_PORT", port, true);
    //Creates the socket
#ifdef IPV6
    s = socket(PF_INET6, SOCK_STREAM, 0);
#else
    s = socket(PF_INET, SOCK_STREAM, 0);
#endif

    {
        /*
        This futile system call is here just because a debian mantainer decided
        that the default behavior must be to listen only to IPv6 connections
        excluding IPv4 ones.
        So this restores the logic and normal behavior of accepting connections
        without being racist about the client's address.
        */
        int val = 0;
#ifdef IPV6
        setsockopt(s, IPPROTO_IPV6, IPV6_V6ONLY, (char *)&val, sizeof(val));
#endif

        val = 1;
        //Makes port reusable immediately after termination.
        if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)) < 0) {
            perror("ruseaddr(any)");
#ifdef IPV6
            char *suggestion = "If you don't have IPv6 support in kernel, try recompiling weborf, removing the line '#define IPV6' from options.h\n";
            write(2, suggestion, strlen(suggestion));
#endif
            return 1;
        }
    }


#ifdef IPV6
    ipAddrL = farAddrL = sizeof(struct sockaddr_in);
    //Bind
    memset(&locAddr, 0, sizeof(locAddr));
    locAddr.sin6_family = AF_INET6;
    locAddr.sin6_port = htons(strtol(port, NULL, 0));
    if (ip == NULL) { //Default ip, listens to all the interfaces
        locAddr.sin6_addr = in6addr_any;
    } else { //Custom ip
        if (inet_pton(AF_INET6, ip, &locAddr.sin6_addr) == 0) {
            printf("Invalid IP address: %s\n", ip);
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
    locAddr.sin_family = AF_INET;	//Internet socket

    {
        //Check the validity of port param and uses it
        unsigned int p = strtol(port, NULL, 0);
        if (p < 1 || p > 65535) {
            printf("Invalid port number: %d\n", p);
            exit(4);
        }
        locAddr.sin_port = htons(p);
    }

    if (ip == NULL)
        ip = "0.0.0.0"; //Default ip address
    if (inet_aton(ip, &locAddr.sin_addr) == 0) { //Converts ip to listen in binary format
        printf("Invalid IP address: %s\n", ip);
        exit(2);
    }
#ifdef SOCKETDBG
    syslog(LOG_INFO, "Listening on address: %s:%d",
           inet_ntoa(locAddr.sin_addr), ntohs(locAddr.sin_port));
#endif


    ipAddrL = farAddrL = sizeof(struct sockaddr_in);


    //Bind
    if (bind(s, (struct sockaddr *) &locAddr, ipAddrL) == -1) {
        perror("trying to bind");
#ifdef SOCKETDBG
        syslog(LOG_ERR, "Port %d already in use", ntohs(locAddr.sin_port));
#endif
        exit(3);
    }
#endif

    set_new_uid(uid);

    //init the queue for opened sockets
    if (q_init(&queue, MAXTHREAD + 1) != 0)
        exit(NOMEM);

    //Starts the 1st group of threads
    init_thread_attr();
    init_threads(INITIALTHREAD);

    {
        //Starts the monitoring thread, to close unused threads
        pthread_t t_id; //Unused var
        pthread_create(&t_id, NULL, t_shape, (void *) NULL);
    }

    //Handle SIGINT and SIGTERM
    signal(SIGINT, quit);
    signal(SIGTERM, quit);

    //Prints queue status with this signal
    signal(SIGUSR1, print_queue_status);


    listen(s, MAXQ); //Listen to the socket

    //Infinite cycle, accept connections
#ifdef IPV6
    while ((s1 = accept(s, (struct sockaddr *) &farAddr, &farAddrL)) != -1) {
#else
    while ((s1 = accept(s, (struct sockaddr *) &farAddr, (socklen_t *) & farAddrL)) != -1) {
#endif

        if (s1 >= 0 && q_put(&queue, s1)!=0) { //Adds s1 to the queue
#ifdef REQUESTDBG
            syslog(LOG_ERR,"Not enough resources, dropping connection...");
#endif
            close(s1);
        }

        //Start new thread if needed
        if (thread_info.free <= LOWTHREAD && thread_info.free<MAXTHREAD) { //Need to start new thread
            if (thread_info.count + INITIALTHREAD < MAXTHREAD) { //Starts a group of threads
                init_threads(INITIALTHREAD);
            } else { //Can't start a group because the limit is close, starting less than a whole group
                init_threads(MAXTHREAD - thread_info.count);
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

/**
Sets the base dir, making sure that it is really a directory.
 */
void setBasedir(char * bd) {
    struct stat stat_buf;
    stat(bd, &stat_buf);

    if (!S_ISDIR(stat_buf.st_mode)) {
        //Not a directory
        printf("%s must be a directory\n",bd);
        exit(1);
    }
    basedir = bd;
}

/**
Checks that the authentication socket exists and is a unix socket
*/
void set_authsocket(char *u_socket) {
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


    authsock = u_socket;
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
This function, executed as a thread, terminates threads if there are too much free threads.

It works polling the number of free threads and writing an order of termination if too much of them are free.

Policies of this function (polling frequence and limit for free threads) are defined in options.h
 */
void *t_shape(void *nulla) {

    for (;;) {
        sleep(THREADCONTROL);

        //pthread_mutex_lock(&thread_info.mutex);
        if (thread_info.free > MAXFREETHREAD) {	//Too much free threads, terminates one of them
            //Write the termination order to the queue, the thread who will read it, will terminate
            q_put(&queue,-1);
        }
        //pthread_mutex_unlock(&thread_info.mutex);
    }
    return NULL; //make gcc happy
}

/**
Will print the internal status of the queue.
This function is triggered by SIGUSR1 signal.
*/
void print_queue_status() {

    //Lock because the values are read many times and it's needed that they have the same value all the times

    if ( pthread_mutex_trylock(&queue.mutex)==0) {
        printf("Queue is unlocked\n");
        pthread_mutex_unlock(&queue.mutex);
    } else {
        printf("Queue is locked\n");
    }


    if ( pthread_mutex_trylock(&thread_info.mutex)==0) {
        printf("thread_info is unlocked\n");
        pthread_mutex_unlock(&thread_info.mutex);
    } else {
        printf("thread_info is locked\n");
    }

    pthread_mutex_lock(&thread_info.mutex);
    printf("=== Queue ===\ncount:      %d\tsize:       %d\nhead:       %d\ttail:       %d\nwait_data:  %d\twait_space: %d\n=== Threads ===\nMaximum:    %d\nStarted:    %d\nFree:       %d\nBusy:       %d\n",queue.num,queue.size,queue.head,queue.tail,queue.n_wait_dt,queue.n_wait_sp,MAXTHREAD,thread_info.count,thread_info.free,thread_info.count-thread_info.free);
    pthread_mutex_unlock(&thread_info.mutex);
}
