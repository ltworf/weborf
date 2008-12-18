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
				if (sendPage(sock,page)<0){ //Invia la pagina
					break;//Verificato errore nell'invio di un errore.
				}
			}
		}
				
		syslog(LOG_DEBUG,"Thread %d: Closing socket with client",id);
		close(sock);//Chiude la socket
		free_thread(id);//Imposta questo thread come libero
	}
	return NULL;
}

/**
Questa funzione effettua alcune modifiche sulla stringa che contiene l'url.
*/
void modURL(char* url) {
	strReplace(url,"../",'\0');
	
	strReplace(url,"%20",' ');//Escape per gli spazi
	//TODO altre sequenze di escape
	//TODO AbsoluteURI: beccare la stringa a partire dal 3o / se è in formato assoluto...
}

/**
Questo metodo determina la pagina richiesta dal client e la invia
*/
int sendPage(int sock,char * page) {
	
	
	char * params=NULL;//Puntatore alla stringa che contiene i parametri
	int p_start=nullParams(page);
	if (p_start!=-1) params=page+p_start+sizeof(char);//Faccio puntare ai parametri
	
	modURL(page);//Operazioni sulla stringa che contiene l'url
	syslog (LOG_DEBUG,"URL changed into %s",page);
		
	//syslog(LOG_INFO,"Parametri %s",params);
	
	int retval;
	
	if (estensione(page,".php")==0) {//File php
		retval= execPage(sock,page,params,"php");
	} else if (estensione(page,".bsh")==0) {//Script bash
		retval=execPage(sock,page,params,"bash");
	} else {//Nessuna estensione particolare, invia normalmente il file
		retval= writePage(sock,page);
	}
	
	int err_res;
	
	if (retval==0) return 0;
	else if (retval==ERR_BRKPIPE) err_res = send_err(sock,500,"Internal server error");
	else if (retval==ERR_FILENOTFOUND) err_res = send_err(sock,404,"Page not found");
	else if (retval==ERR_NOMEM) err_res = send_err(sock,503,"Service Unavailable");
	
	return err_res;
}
/**
Metodo per eseguire uno script con un dato interprete e mandare il risultato su una socket
*/
int execPage(int sock, char * file, char * params,char * executor) {
	
	char * strfile=malloc(INBUFFER+strlen(basedir));//Alloco il buffer per il nome del file
	
	if (strfile==NULL)return ERR_NOMEM;//Se non alloca la memoria ritorna un serice unavailable
	
	//Creo la stringa con il nome del file
	sprintf(strfile,"%s%s",basedir,file);//Compone la stringa con il path e il nome del file
	syslog(LOG_INFO,"Executing file %s",strfile);
	
	//Controlla se il file esiste
	if (file_exists(strfile)!=0) return ERR_FILENOTFOUND;
	
	int wpid;//Pid del figlio
	int wpipe[2];//Descrittori di file della pipe
	
	pipe(wpipe);//Crea la pipe per comunicare con il figlio
	wpid=fork();//Forks
	if (wpid<0) {//Error
		syslog(LOG_ERR,"Unable to fork to execute the file %s",strfile);
		return ERR_NOMEM;
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
			exit(1);
		} else {
			syslog(LOG_DEBUG,"Executing %s interpreter for %s",executor,strfile);
			execlp(executor,executor,strfile,(char *)0);
			syslog(LOG_ERR,"Execution of the %s interpreter failed",executor);
			exit(1);
		}
	} else { //Processo padre, legge dalla pipe e invia
		
		int state;
		waitpid (wpid,&state,0); //Aspetta che l'interprete termini
		if (state==0) {
			//Buffer molto grande, deve contenere l'intero output
			char* scrpt_buf=malloc(MAXSCRIPTOUT);
		
			int reads=read(wpipe[0],scrpt_buf,MAXSCRIPTOUT);
			int wrote=0;
			send_http_header(sock,reads);
			wrote=write (sock,scrpt_buf,reads);
			if (wrote<0) syslog(LOG_ERR,"The client closed the socket");
			free(scrpt_buf);
		} else {
			return ERR_BRKPIPE;//L'interprete è fallito. Internal server error
		}
	}
	return 0;
}

/**
Questa funzione scrive sulla socket una pagina html contenente l'elenco dei file
all'interno della directory specificata
*/
int writeDir(int sock, char* page) {
	
	char* html=malloc(MAXSCRIPTOUT);//Spazio per la pagina html
	if (html==NULL) {//Non c'è memoria e ritorna
		return ERR_NOMEM;
	}
	
	if (list_dir (page,html,MAXSCRIPTOUT)<0) {
		return ERR_FILENOTFOUND;
	} else {//Se tutto va bene invia la lista dei file
		int pagelen=strlen(html);
		send_http_header(sock,pagelen);
		write(sock,html,pagelen);
	}
	
	free(html);//Libera la memoria usata per memorizzare la pagina
	
	return 0;
}

/**
Questo metodo legge il file e lo scrive sulla socket
*/
int writePage(int sock,char * file) {
	char * strfile=malloc(strlen(file)+strlen(basedir)+1);//Alloco il buffer per il nome del file
	
	if (strfile==NULL)return ERR_NOMEM;//Se non alloca la memoria ritorna un serice unavailable

	
	//Creo la stringa con il nome del file
	sprintf(strfile,"%s%s",basedir,file);//Compone la stringa con il path e il nome del file
	
	
	
	{//Blocco che controlla se è stata richiesta una directory invece di un file
		syslog(LOG_DEBUG,"File: %s",strfile);
		
		int f_mode=fileIsA(strfile);//Ottiene il mode dell'elemento
		if (S_ISDIR(f_mode)) {//Controlla se è una directory.
			
			char* newpath=malloc(strlen(file)+strlen(INDEX)+2);
			if (newpath==NULL) return ERR_NOMEM;
			
			if (estensione(strfile,"/")==0) {
				sprintf(newpath,"%s%s",file,INDEX);//Aggiunge index all'url.
			} else {
				sprintf(newpath,"%s/%s",file,INDEX);//Aggiunge index all'url.
			}
			
			int res=writePage(sock,newpath);//Invia l'index
			free(newpath);
			
			if (res!=ERR_FILENOTFOUND) {
				free(strfile);
				return res; //Restituisce il risultato
			} else {
				
				char* dir=strfile;
				
				/*int changed=0;
				if (estensione(strfile,"/")!=0) {
					dir=malloc(strlen(strfile)+2);
					
					if (dir==NULL) {//Non c'è abbast memoria
						free(strfile);
						return ERR_NOMEM;
					}
					changed=1;
					sprintf("%s/",strfile);
				}*/
				
				int dires=writeDir(sock,dir);
				free(strfile);
				//if(changed) free(dir);
				return dires;
			}
		}
	}
	
	//syslog(LOG_INFO,"Loading file %s",strfile);
	
	char* buf=malloc(FILEBUF);//Alloca un Buffer
	
	if (buf==NULL)return ERR_NOMEM;//Se non alloca la memoria ritorna un serice unavailable
	
	int fp;//Puntatore al file su disco
	
	fp=open(strfile,O_RDONLY);
	
	if (fp<0) {//Errore
		
		free((void *) strfile);
		free(buf);
			
		return ERR_FILENOTFOUND;
	}
	

	send_http_header(sock,getFileSize(fp));
	
	int reads;
	int wrote;
	
	while((reads=read(fp, buf, FILEBUF))>0) { 
		wrote=write(sock,buf,reads);
		if (wrote!=reads) {//Si è verificato un errore di scrittura sulla socket
			syslog(LOG_ERR,"Unable to send %s: error writing to the socket",file);
			break;
		}
	}	
	close(fp);//Chiude il file locale
	
	free((void *) strfile);
	free(buf);
	return 0;
}

/**
Invia un errore al client
*/
int send_err(int sock,int err,char* descr) {
	
	//Preparo la pagina HTML con l'errore
	char * page=malloc(MAXSCRIPTOUT);
	
	if (page==NULL) return ERR_NOMEM;
	
	sprintf(page,"%s <H1>Error %d</H1>%s %s",HTMLHEAD,err,descr,HTMLFOOT);
	
	//Preparo l'header http
	char * head=malloc(HEADBUF);
	
	if (head==NULL) {
		free(page);
		return ERR_NOMEM;
	}
	
	sprintf(head,"HTTP/1.1 %d %s: Weborf (GNU/Linux)\r\nContent-Length: %d\r\n\r\n",err,descr ,strlen(page));
	
	
	int head_len = strlen(head);
	int page_len = strlen(page);
	
	//Invio l'header
	if (write (sock,head,head_len)!=head_len) {
		free(page);
		free(head);
		return -4;
	}
	
	//Invio la pagina
	if (write(sock,page,page_len)!=page_len) {
		free(page);
		free(head);
		return -4;
	}
	
	syslog(LOG_ERR,"Error %d: %s",err,descr);
	
	free(page);
	free(head);
	return 0;
}

/**
Questa funzione invia un header http alla socket specificata
size indica la dimensione
*/
int send_http_header(int sock,int size) {
	//Invio l'header http
	
	char *head=malloc(HEADBUF);
	if (head==NULL)return ERR_NOMEM;//Niente memoria, dice che il server non è disponibile
	
	sprintf(head,"HTTP/1.1 200 OK Server: Weborf (GNU/Linux)\r\nContent-Length: %d\r\n\r\n", size);	
	
	int len_head=strlen(head);
	int wrote=write (sock,head,len_head);
	free(head);
	if (wrote!=len_head) return ERR_BRKPIPE;//Problema del server
	return 0;
}
