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
 
@author Salvo "LtWorf" Tomaselli <ltworf@galileo.dmi.unict.it>
@author Giuseppe Pappalardo <pappalardo@dmi.unict.it>
@author Salvo Rinaldi <salvin@bluebottle.com> 
*/

#include "listener.h"
#include "instance.h"
#include "queue.h"
#include "options.h"
#include "utils.h"

syn_queue_t queue; //Queue for opened sockets


pthread_mutex_t m_free;//Mutex to modify t_free
unsigned int t_free=0;//Free threads

pthread_mutex_t m_thread_c;//Mutex to modify thread_c
unsigned int thread_c=0;//Number of threads

char * basedir=BASEDIR;//Base directory

uid_t uid=ROOTUID;//Uid to use after bind

pthread_attr_t t_attr;//thread's attributes


/**
Increases or decreases the number of current active thread.
This function is thread safe.

Notice that this will not start or stop threads, just change the value of thread_c.
*/
void chn_thread_count(short int val) {
	
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
*/
void init_threads() {
	static long int id=1;
	//t_free=MAXTHREAD;
	int i;
	
	pthread_t t_id;//Unused var, thread's system id
	
	pthread_mutex_lock(&m_free);
	
	for (i = 1; i <= INITIALTHREAD; i++) {
		pthread_create(&t_id, &t_attr, instance, (void*)(id++));
		
	}
		chn_thread_count(INITIALTHREAD);//Increases the counter of active threads
		t_free+=INITIALTHREAD;
		syslog(LOG_DEBUG,"There are %d free threads",t_free);
	pthread_mutex_unlock(&m_free);
}

/**
This function inits the logger.
Will use syslogd
*/
void init_logger() {
	openlog("weborf",LOG_ODELAY,LOG_DAEMON);
	syslog(LOG_INFO,"Startig server...");
}

int main(int argc, char * argv[]) {
	
	init_logger();//Inits the logger
	
	int s, s1;//Socket
	
	char *ip=LOCIP;//IP addr with default value
	char *port=PORT;//port with default value
	{//Block to read command line
		int i;
		for (i=1;i<argc;i++) {
			if (argv[i][0]==45 && argv[i][1]==45) { //Long argument
				if (0 != strstr(argv[i],"version")) version();
				else if (0 != strstr(argv[i],"help")) help();
				else if (0 != strstr(argv[i],"port") && i+1<argc) port=argv[++i];
				else if (0 != strstr(argv[i],"ip") && i+1<argc) ip=argv[++i];
				else if (0 != strstr(argv[i],"basedir") && i+1<argc) setBasedir(argv[++i]);
				else if (0 != strstr(argv[i],"moo")) moo();
				else invalidOption(argv[i]);
				
			} else if (argv[i][0]==45) { //Short argument
				if (0 != strstr(argv[i], "v")) version();
				else if (0 != strstr(argv[i], "h")) help();
				else if (0 != strstr(argv[i], "p") && i+1<argc) port=argv[++i];
				else if (0 != strstr(argv[i], "i") && i+1<argc) ip=argv[++i];
				else if (0 != strstr(argv[i], "b") && i+1<argc) setBasedir(argv[++i]);
				else if (0 != strstr(argv[i], "u") && i+1<argc) {//Sets uid var
					uid=strtol( argv[++i] , NULL, 0 );
				}
				else invalidOption(argv[i]);
			} else {
				invalidOption(argv[i]);
			}
		}
	}
	
	
	printf("Weborf\n");
	printf("This program comes with ABSOLUTELY NO WARRANTY.\n");
	printf("This is free software, and you are welcome to redistribute it\n");
	printf("under certain conditions.\nFor details see the GPLv3 Licese.\n");
	printf("Run %s --help to see the options\n",argv[0]);


	struct sockaddr_in locAddr, farAddr;
	int farAddrL, ipAddrL; 

	//Creates the socket
	s = socket(PF_INET, SOCK_STREAM, 0);
	
	
	int val=1;
	//Makes port reusable immediately after termination.
	if(setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)) < 0){
		perror("ruseaddr(any)");
		return 1;
	}	

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
	
	
	if (inet_aton(ip, &locAddr.sin_addr)==0) {//Converts ip to listen in binary format
		printf("Invalid IP address: %s\n",ip);
		exit(2);
	}
	
	syslog(LOG_INFO,"Listening on address: %s:%d", inet_ntoa(locAddr.sin_addr),ntohs(locAddr.sin_port));
	
	
	ipAddrL = farAddrL = sizeof(struct sockaddr_in);
	
	//Bind
	if ( bind(s, (struct sockaddr *) &locAddr, ipAddrL) == -1 ) {
		perror("trying to bind");
		syslog(LOG_ERR,"Port %d already in use",ntohs(locAddr.sin_port));
		exit(-1);
	}
	
	
	//Changes UID.
	if (uid!=ROOTUID) {
		if (setuid(uid)==0) {
			//Uid changed correctly
			syslog(LOG_INFO,"Changed uid. New one is %d",uid);
		} else {
			//Not enough permissions i guess...
			syslog(LOG_ERR,"Unable to change uid.");
			perror("Unable to change uid");
			exit(9);
		}
	}
	
	//init the queue for opened sockets
	q_init(&queue,MAXTHREAD+1);

	//Starts the 1st group of threads
	init_thread_attr();
	init_threads();
	
	{//Starts the monitoring thread, to close unused threads
		pthread_t t_id;//Unused var
		pthread_create(&t_id, NULL, t_shape, (void*)NULL);
	}

	//Handles SIGINT
	signal(SIGINT, chiudi);
	

	listen(s, MAXQ);//Listen to the socket

	int loc_free=t_free;//Local t_free, used to avoid deadlock

	//Infinite cycle, accept connections
	while ((s1 = accept(s, (struct sockaddr *) &farAddr,(socklen_t *)&farAddrL)) != -1) {
		
		syslog(LOG_INFO,"Connection from %s", inet_ntoa(farAddr.sin_addr));
		
		if (s1>=0  && loc_free>0) {//Adds s1 to the queue
			q_put(&queue, s1);
		} else {//Closes the socket if there aren't enough free threads.
			syslog(LOG_ERR,"Not enough free threads, dropping connection...");
			close(s1);
		}

		//Legge il numero di thread liberi
		pthread_mutex_lock(&m_free);
		loc_free=t_free;
		pthread_mutex_unlock(&m_free);
		
		//Start new thread if needed
		if (loc_free<=LOWTHREAD) {//Need to start new thread
			if (thread_c+INITIALTHREAD<MAXTHREAD) init_threads();
		}
	}
	
	return 0;

}

/**
SIGINT signal handler, Ctrl+C
*/
void chiudi() {
	syslog(LOG_INFO,"Stopping server...");
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
		
		if (loc_free>MAXFREETHREAD) {//Too much free threads, terminates one of them
			q_put(&queue, -1);//Write the termination order to the queue, the thread who will read it, will terminate
			chn_thread_count(-1);//Decreases the number of free total threads
		}
	}
}
