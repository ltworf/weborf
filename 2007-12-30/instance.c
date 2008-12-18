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
@author Salvo Rinaldi <salvin@bluebottle.com>
 */
#include "instance.h"
#include "queue.h"
#include "options.h"
#include "mystring.h"
#include "utils.h"

extern syn_queue_t queue; //Coda delle socket accettate

extern pthread_mutex_t t_mutex;//Mutex per modificare quel valore
extern unsigned int t_free;//Thread liberi

extern char* basedir;//Basedir

/**
Imposta il thread id come non libero
*/
void unfree_thread(int id) {
	pthread_mutex_lock(&t_mutex);
	t_free--;
	syslog(LOG_DEBUG,"There are %d free threads",t_free);
	pthread_mutex_unlock(&t_mutex);	
}

/**
Imposta il thread id come libero
*/
void free_thread(int id) {
	pthread_mutex_lock(&t_mutex);
	t_free++;
	syslog(LOG_DEBUG,"There are %d free threads",t_free);
	pthread_mutex_unlock(&t_mutex);
}

/**
Gestore per SIGPIPE
Non fa niente in quanto è sufficiente il valore restituito da write.
*/
void piperr() {
	//Non faccio nulla
}

/**
Metodo eseguito all'avvio dei nuovi thread.
Preleva le socket aperte dalla coda e serve le richieste all'infinito.
Quando non ci sono socket aperte pronte va in sleep
*/
void * instance(void * nulla){
	int id=(int)nulla;
	syslog(LOG_DEBUG,"Starting thread %d",id);
	
	signal(SIGPIPE, piperr);//Gestisce il segnale SIGPIPE
	
	int bufFull;
	char buf[INBUFFER];
	char * req;
	char * page;
	char * lasts;
	int sock;//Socket per comunicare con il client
	for (;;) {//Ciclo infinito
		q_get(&queue, &sock);//Ottiene una socket aperta dalla coda
		unfree_thread(id);//Imposta questo thread come occupato
		
		if (sock<0) {//Richiesta di terminare il thread
			syslog(LOG_DEBUG,"Terminating thread %d",id);
			pthread_exit(0);
		}
		
		
		syslog(LOG_DEBUG,"Thread %d: Reading from socket",id);
		
		
		while ((bufFull=read(sock, buf,INBUFFER))>0){
			if(bufFull==INBUFFER){ //Buffer overflow
				syslog(LOG_ERR,"Buffer overflow");
				break;
			}
			req=strtok_r(buf," ",&lasts);
			if (!strcasecmp("get",req)){
				page=strtok_r(NULL," ",&lasts);
				syslog(LOG_INFO,"Requested page: %s to Thread %d",page,id);
				sendPage(sock,page);//Invia la pagina
			}
		}
				
		syslog(LOG_DEBUG,"Thread %d: Closing socket with client",id);
		close(sock);//Chiude la socket
		free_thread(id);//Imposta questo thread come libero
	}
	return NULL;
}

/**
Questo metodo determina la pagina richiesta dal client e la invia
*/
void sendPage(int sock,char * page) {
	
	
	//TODO AbsoluteURI: beccare la stringa a partire dal 3o / se è in formato assoluto...
	
	if (estensione(page,"/")==0) {//Cartella, manda l'index
		
		//Alloco il buffer per l'url completo di index
		char * strfile=malloc(strlen(page)+strlen(INDEX)+1);
	
		//Creo la stringa con il nome del file
		sprintf(strfile,"%s%s",page,INDEX);
		
		//Invia l'index per la cartella richiesta
		if (writePage(sock,strfile)<0) {//Manca l'index
			syslog(LOG_ERR,"Missing %s file",INDEX);
			
			//Percorso assoluto della directory
			char * abs_dir_path=malloc(strlen(basedir)+strlen(page)+1);
			sprintf(abs_dir_path,"%s%s",basedir,page);
			
			syslog(LOG_INFO,"Scanning dir %s",abs_dir_path);
			
			char* html=malloc(MAXSCRIPTOUT);//Spazio per la pagina html
			if (list_dir (abs_dir_path,html,MAXSCRIPTOUT)<0) {
				send404(sock);
			} else {//Se tutto va bene invia la lista dei file
				send_http_header(sock,strlen(html));
				write(sock,html,strlen(html));
			}
			free(abs_dir_path);//Libera la memoria per il percorso assoluto del file
			free(html);//Libera la memoria usata per memorizzare la pagina
		}
		
		//Libera lo spazio usato dall'url
		free(strfile);
		return;
	}
	
	char * params=NULL;//Puntatore alla stringa che contiene i parametri
	int p_start=nullParams(page);
	if (p_start!=-1) params=page+p_start+sizeof(char);//Faccio puntare ai parametri
	
	//syslog(LOG_INFO,"Parametri %s",params);
	
	
	if (estensione(page,".php")==0) {//File php
		if (execPage(sock,page,params,"php")<0) send404(sock);
	} else if (estensione(page,".bsh")==0) {//Script bash
		if (execPage(sock,page,params,"bash")<0) send404(sock);
	} else {//Nessuna estensione particolare, invia normalmente il file
		if (writePage(sock,page)<0) send404(sock);
	}
}

/**
Metodo per eseguire uno script con un dato interprete e mandare il risultato su una socket
*/
int execPage(int sock, char * file, char * params,char * executor) {
	
	char * strfile=malloc(INBUFFER+strlen(basedir));//Alloco il buffer per il nome del file
	
	//Creo la stringa con il nome del file
	sprintf(strfile,"%s%s",basedir,file);//Compone la stringa con il path e il nome del file
	syslog(LOG_INFO,"Executing file %s",strfile);
	
	if (file_exists(strfile)!=0) return -2;
	
	int wpid;//Pid del figlio
	int wpipe[2];//Descrittori di file della pipe
	
	pipe(wpipe);//Crea la pipe per comunicare con il figlio
	wpid=fork();//Forks
	if (wpid<0) {//Error
		syslog(LOG_ERR,"Unable to fork to execute the file %s",strfile);
		return -3;
	}
	if (wpid==0) { //Esegue l'interprete della pagina
		close (wpipe[0]); //Closes unused end of the pipe
		fclose (stdout); //Closing the stdout
		fclose (stderr);
		dup(wpipe[1]); //Redirects the stdout
		if (params!=NULL) { 
			/*int args= splitParams(params);//Trasforma i parametri in tante stringhe
			char* prms[args+2];
			{//Fa puntare alle varie stringhe
			int in;
			prms[0]=executor;//Points to the program name
			char* next=params;
			for (in=1;in<=args;in++) {
			prms[in]=next;
			next=findNext(next);
		}
			prms[args+1]=(char *)0;//Null string
				 
		}
			execvp(executor,prms);
			*/
			
			//Esegue con i parametri passati alla pagina
			execlp(executor,executor,strfile,params,(char *)0);
			
			syslog(LOG_ERR,"Execution of the %s interpreter failed",executor);
			return -1;
		} else {
			syslog(LOG_DEBUG,"Executing %s interpreter for %s",executor,strfile);
			execlp(executor,executor,strfile,(char *)0);
			syslog(LOG_ERR,"Execution of the %s interpreter failed",executor);
			return -1;
		}
	} else { //Processo padre, legge dalla pipe e invia
		
		int state;
		waitpid (wpid,&state,0); //Aspetta che l'interprete termini
		
		char scrpt_buf[MAXSCRIPTOUT];//Buffer molto grande, deve contenere l'intero output
		int reads=read(wpipe[0],scrpt_buf,MAXSCRIPTOUT);
		int wrote=0;
		send_http_header(sock,reads);
		wrote=write (sock,scrpt_buf,reads);
		if (wrote<0) syslog(LOG_ERR,"The client closed the socket");
		
	}
	return 0;
}

/**
Questo metodo legge il file e lo scrive sulla socket
*/
int writePage(int sock,char * file) {
	char * strfile=malloc(INBUFFER+strlen(basedir));//Alloco il buffer per il nome del file
	
	//Creo la stringa con il nome del file
	sprintf(strfile,"%s%s",basedir,file);//Compone la stringa con il path e il nome del file
	//syslog(LOG_INFO,"Loading file %s",strfile);
	
	char buf[FILEBUF];//Buffer di un carattere
	int fp;//Puntatore al file su disco
	
	fp=open(strfile,O_RDONLY);
	
	if (fp<0) {//Errore
		return -1;
	}
	

	send_http_header(sock,getFileSize(fp));
	
	int reads;
	int wrote;
	
	while((reads=read(fp, buf, FILEBUF))>0) { 
		wrote=write(sock,buf,reads);
		if (wrote<0) {//Si è verificato un errore di scrittura sulla socket
			syslog(LOG_ERR,"Unable to send %s: error writing to the socket",file);
			break;
		}
	}	
	close(fp);//Chiude il file locale
	
	free((void *) strfile);
	return 0;
}

/**
Invia un errore 404 al client
*/
void send404(int sock) {
	//Invio l'header http
	char head[HEADBUF];
	sprintf(head,"HTTP/1.1 404 Page not found Server: Weborf (GNU/Linux)\r\nContent-Length: %d\r\n\r\n", strlen(ERR404));
	write (sock,head,strlen(head));
		
	write(sock,ERR404,strlen(ERR404));
	syslog(LOG_ERR,"Can't open requested page");	
	return;
}

/**
Questa funzione invia un header http alla socket specificata
size indica la dimensione
*/
void send_http_header(int sock,int size) {
	//Invio l'header http
	
	char head[HEADBUF];
	sprintf(head,"HTTP/1.1 200 OK Server: Weborf (GNU/Linux)\r\nContent-Length: %d\r\n\r\n", size);	
	write (sock,head,strlen(head));
}
