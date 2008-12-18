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
 */

#include "listener.h"
#include "instance.h"
#include "queue.h"
#include "options.h"

syn_queue_t queue; //Coda delle socket accettate



pthread_mutex_t t_mutex;//Mutex per modificare quel valore
unsigned int t_free=0;//Thread liberi
unsigned int thread_c=0;//Numero di thread avviati

pthread_t thrd[MAXTHREAD];


/**Avvia tutti i thread in una volta*/
void init_threads() {
	static int id=0;
	t_free=MAXTHREAD;
	int i;
	for (i = 1; i <= MAXTHREAD; i++) {
		pthread_create(&thrd[i], NULL, instance, (void*)(id++));
		thread_c++;
	}

	/*
	pthread_mutex_lock(&t_mutex);
		t_free+=INITIALTHREAD;
		syslog(LOG_DEBUG,"There are %d free threads",t_free);
	pthread_mutex_unlock(&t_mutex);
	*/
}

/**
Questo metodo inizializza il logger.
Utilizza syslogd
*/
void init_logger() {
	openlog("weborf",LOG_ODELAY,LOG_DAEMON);
	syslog(LOG_INFO,"Startig server...");
}

int main() {

printf("Weborf Copyright (C) 2007  Salvo \"LtWorf\" Tomaselli\n");
printf("This program comes with ABSOLUTELY NO WARRANTY.\n");
printf("This is free software, and you are welcome to redistribute it\n");
printf("under certain conditions.\nFor details see the GPLv3 Licese.\n");

	init_logger();//Inizializza il logger


	int s, s1;//Socket


	struct sockaddr_in locAddr, farAddr;
	int farAddrL, ipAddrL; 

	//Crea la socket		
	s = socket(PF_INET, SOCK_STREAM, 0);
	
	// Costruzione indirizzo socket
	locAddr.sin_family = AF_INET;//Socket su internet
	locAddr.sin_port = htons(PORT);//Imposta la porta da usare
	inet_aton(LOCIP, &locAddr.sin_addr);//Converte l'IP locale in formato binario
	syslog(LOG_INFO,"Listening on address: %s", inet_ntoa(locAddr.sin_addr));
	syslog(LOG_INFO,"Listening port: %d", ntohs(locAddr.sin_port));
	
	ipAddrL = farAddrL = sizeof(struct sockaddr_in);
	
	//Bind
	if ( bind(s, (struct sockaddr *) &locAddr, ipAddrL) == -1 ) {
		perror("trying to bind");
		syslog(LOG_ERR,"Port %d already in use",PORT);
		exit(-1);
	}

	//inizializza la coda per i socket
	q_init(&queue,MAXTHREAD+1);

	
	//Inizializzo i primi threads
	init_threads();

	//Imposta il gestore per i segnali
	signal(SIGINT, chiudi);
	

	listen(s, MAXQ);//Ascolta sulla socket

	//Inizia ad ascoltare sulla socket, e crea le socket sulle porte alte.
	while ((s1 = accept(s, (struct sockaddr *) &farAddr,(socklen_t *)&farAddrL)) != -1) {
		
		syslog(LOG_INFO,"Connection from %s", inet_ntoa(farAddr.sin_addr));

		/* Avvio di thread quando servono
		if (t_free<=2) {//Pochi thread liberi, ne avvia altri se possibile
			if (thread_c+INITIALTHREAD<MAXTHREAD) init_threads();
		}*/

		//Aggiunge s1 nella coda
		if (s1>=0  && t_free>0 ) q_put(&queue, s1);
		else close(s1);//Chiude la socket per mancanza di thread disponibili
	}

	return 0;

}


/**
Chiude tutti i thread, inviando -1 come socket.
Quando ogni thread leggerà tale valore terminerà
*/
void stop_threads() {
	int i;
	for (i=1;i<=MAXTHREAD;i++) {
		q_put(&queue, -1);
	}
	sleep(4);
	exit(0);
}

/**
Gestore di segnali attivato alla pressione del control C
*/
void chiudi() {
	
	stop_threads();
}
