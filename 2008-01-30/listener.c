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


pthread_mutex_t m_free;//Mutex per modificare quel valore
unsigned int t_free=0;//Thread liberi

pthread_mutex_t m_thread_c;//Mutex per modificare quel valore
unsigned int thread_c=0;//Numero di thread avviati

char * basedir=BASEDIR;//Cartella di base.

uid_t uid=ROOTUID;//Uid per fare il setuid

pthread_attr_t t_attr;//Attributi dei thread


/**
Incrementa o decrementa di val il numero di thread attivi
Usa la mutex
*/
void chn_thread_count(short int val) {
	
	pthread_mutex_lock(&m_thread_c);
	thread_c+=val;
	pthread_mutex_unlock(&m_thread_c);
}



/**
Imposta le proprietà dei thread contenute in t_attr come detached
*/

void init_thread_attr() {
	int rc = pthread_attr_init(&t_attr);	
	rc = pthread_attr_setdetachstate(&t_attr, PTHREAD_CREATE_DETACHED);
}




/**Avvia tutti i thread in una volta*/
void init_threads() {
	static int id=1;
	//t_free=MAXTHREAD;
	int i;
	
	pthread_t t_id;//Variabile non usata
	
	pthread_mutex_lock(&m_free);
	
	for (i = 1; i <= INITIALTHREAD; i++) {
		pthread_create(&t_id, &t_attr, instance, (void*)(id++));
		chn_thread_count(1);//Incrementa di uno il numero di thread attivi
		
	}
		t_free+=INITIALTHREAD;
		syslog(LOG_DEBUG,"There are %d free threads",t_free);
	pthread_mutex_unlock(&m_free);
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
	
	
	//Cambio l'uid
	if (uid!=ROOTUID) {
		if (setuid(uid)==0) {
			//Uid cambiato
			syslog(LOG_INFO,"Changed uid. New one is %d",uid);
		} else {
			syslog(LOG_ERR,"Unable to change uid.");
			perror("Unable to change uid");
			exit(9);
		}
	}

	//inizializza la coda per i socket
	q_init(&queue,MAXTHREAD+1);

	
	
	//Inizializzo i primi threads
	init_thread_attr();
	init_threads();
	
	{//Avvia il thread che chiude i thread quando si fanno assai
		pthread_t t_id;//Variabile non usata
		pthread_create(&t_id, NULL, t_shape, (void*)NULL);
	}

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
			syslog(LOG_ERR,"Not enough free threads, dropping connection...");
			close(s1);
		}

		//Legge il numero di thread liberi
		pthread_mutex_lock(&m_free);
		loc_free=t_free;
		pthread_mutex_unlock(&m_free);
		
		//Avvio di thread quando servono
		if (loc_free<=LOWTHREAD) {//Pochi thread liberi, ne avvia altri se possibile
			if (thread_c+INITIALTHREAD<MAXTHREAD) init_threads();
		}
	}

	return 0;

}

/**
Gestore di segnali attivato alla pressione del control C
*/
void chiudi() {
	syslog(LOG_INFO,"Stopping server...");
	closelog();
	
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


/**
Funzione (eseguita come thread separato) che controlla il numero di thread disponibili e decide se terminarne.
 */
void* t_shape(void * nulla) {
	
	int loc_free;
	
	for (;;) {
		sleep(THREADCONTROL);
//		q_put(&queue, -1);
	
		//Legge il numero di thread liberi
		pthread_mutex_lock(&m_free);
		loc_free=t_free;
		pthread_mutex_unlock(&m_free);
		
		if (loc_free>MAXFREETHREAD) {//Troppi thread liberi, le disattiva uno
			q_put(&queue, -1);
			chn_thread_count(-1);//Decrementa di uno il numero di thread attivi
		}

	}
}
