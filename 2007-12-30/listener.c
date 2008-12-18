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

syn_queue_t queue; //Coda delle socket accettate


pthread_mutex_t t_mutex;//Mutex per modificare quel valore
unsigned int t_free=0;//Thread liberi
unsigned int thread_c=0;//Numero di thread avviati

pthread_t thrd[MAXTHREAD];

char * basedir=BASEDIR;//Cartella di base.

/**Avvia tutti i thread in una volta*/
void init_threads() {
	static int id=1;
	//t_free=MAXTHREAD;
	int i;
	
	pthread_mutex_lock(&t_mutex);
	
	for (i = 1; i <= INITIALTHREAD; i++) {
		pthread_create(&thrd[id], NULL, instance, (void*)(id));
		id++;
		thread_c++;
	}
		t_free+=INITIALTHREAD;
		syslog(LOG_DEBUG,"There are %d free threads",t_free);
	pthread_mutex_unlock(&t_mutex);
}

/**
Questo metodo inizializza il logger.
Utilizza syslogd
*/
void init_logger() {
	openlog("weborf",LOG_ODELAY,LOG_DAEMON);
	syslog(LOG_INFO,"Startig server...");
}

int main(int argc, char * argv[]) {


	init_logger();//Inizializza il logger


	int s, s1;//Socket
	
	char *ip=LOCIP;//Indirizzo ip impostato di default
	char *port=PORT;//Porta impostata di default
	{//Blocco in cui vengono letti i parametri dalla riga di comando	
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

	//Crea la socket		
	s = socket(PF_INET, SOCK_STREAM, 0);
	
	int val=1;
	if(setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)) < 0){
		perror("ruseaddr(any)");
		return 1;
	}	

	// Costruzione indirizzo socket
	locAddr.sin_family = AF_INET;//Socket su internet
	
	{//Controlla ed assegna la porta
		unsigned int p=strtol( port , NULL, 0 );
		if (p<1 || p>65535) {
			printf("Invalid port number: %d\n",p);
			exit(2);
		}
		locAddr.sin_port=htons(p);
	}
	
	
	if (inet_aton(ip, &locAddr.sin_addr)==0) {//Converte l'IP locale in formato binario
		printf("Invalid IP address: %s\n",ip);
		exit(2);
	}
	
	syslog(LOG_INFO,"Listening on address: %s", inet_ntoa(locAddr.sin_addr));
	syslog(LOG_INFO,"Listening port: %d", ntohs(locAddr.sin_port));
	
	ipAddrL = farAddrL = sizeof(struct sockaddr_in);
	
	//Bind
	if ( bind(s, (struct sockaddr *) &locAddr, ipAddrL) == -1 ) {
		perror("trying to bind");
		syslog(LOG_ERR,"Port %d already in use",ntohs(locAddr.sin_port));
		exit(-1);
	}

	//inizializza la coda per i socket
	q_init(&queue,MAXTHREAD+1);

	
	//Inizializzo i primi threads
	init_threads();

	//Imposta il gestore per i segnali
	signal(SIGINT, chiudi);
	

	listen(s, MAXQ);//Ascolta sulla socket

	int loc_free=t_free;//Copia locale di t_free, usata per evitare deadlock

	//Inizia ad ascoltare sulla socket, e crea le socket sulle porte alte.
	while ((s1 = accept(s, (struct sockaddr *) &farAddr,(socklen_t *)&farAddrL)) != -1) {
		
		syslog(LOG_INFO,"Connection from %s", inet_ntoa(farAddr.sin_addr));
		
		if (s1>=0  && loc_free>0) {//Aggiunge s1 nella coda
			q_put(&queue, s1);
		} else {//Chiude la socket per mancanza di thread disponibili/errori
			close(s1);
		}

		//Legge il numero di thread liberi
		pthread_mutex_lock(&t_mutex);
		loc_free=t_free;
		pthread_mutex_unlock(&t_mutex);
		
		//Avvio di thread quando servono
		if (loc_free<=LOWTHREAD) {//Pochi thread liberi, ne avvia altri se possibile
			if (thread_c+INITIALTHREAD<MAXTHREAD) init_threads();
		}
	}

	return 0;

}


/**
Chiude tutti i thread, inviando -1 come socket.
Quando ogni thread leggerà tale valore terminerà
*/
void stop_threads() {
	
	int i;
	for (i=1;i<=thread_c;i++) {
		syslog(LOG_DEBUG,"Putting %d thread stop",i);
		q_put(&queue, -1);
		sleep(1);
	}
	sleep(4);
	exit(0);
}

/**
Gestore di segnali attivato alla pressione del control C
*/
void chiudi() {
	syslog(LOG_INFO,"Stopping server...");
	closelog();
	
	//stop_threads();
	exit(0);
}

/**
Questa funzione imposta la base directory, controllando che sia
veramente una directory
 */
void setBasedir(char * bd) {
	int f_mode=fileIsA(bd);//Ottiene il mode dell'elemento
	if (!S_ISDIR(f_mode)) {
		printf("%s must be a directory\n",bd);
		exit(1);
	}
	basedir=bd;
}
