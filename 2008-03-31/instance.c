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
#include "message_dequeue.h"

extern syn_queue_t queue; //Queue for open sockets

extern pthread_mutex_t m_free;//Mutex to modify t_free
extern unsigned int t_free;//free threads

extern char* basedir;//Basedir

/**
Set thread with id as non-free
*/
void unfree_thread(long int id) {
	pthread_mutex_lock(&m_free);
	t_free--;
	syslog(LOG_DEBUG,"There are %d free threads",t_free);
	pthread_mutex_unlock(&m_free);	
}

/**
Set thread with id as free
*/
void free_thread(long int id) {
	pthread_mutex_lock(&m_free);
	t_free++;
	syslog(LOG_DEBUG,"There are %d free threads",t_free);
	pthread_mutex_unlock(&m_free);
}

/**
Metodo eseguito all'avvio dei nuovi thread.
Preleva le socket aperte dalla coda e serve le richieste all'infinito.
Quando non ci sono socket aperte pronte va in sleep
*/
void * instance(void * nulla){
	long int id=(long int)nulla;//Set thread's id
	syslog(LOG_DEBUG,"Starting thread %ld",id);
	
	signal(SIGPIPE, SIG_IGN);//Ignores SIGPIPE
	
	int bufFull;
	char buf[INBUFFER+1];
	char * req;//Method of the HTTP request
	char * page;//Page to load
	char * lasts;//Used by strtok_r
	
	char * param;//HTTP parameter
	
	int sock;//Socket per comunicare con il client
	
	bool post;
	
	while (true) {//Ciclo infinito
		q_get(&queue, &sock);//Gets a socket from the queue coda
		unfree_thread(id);//Sets this thread as busy
		
		if (sock<0) {//Was not a socket but a termination order
			syslog(LOG_DEBUG,"Terminating thread %ld",id);
			pthread_exit(0);
		}
		
		
		syslog(LOG_DEBUG,"Thread %ld: Reading from socket",id);
		
		
		while ((bufFull=read(sock, buf,INBUFFER))>0){//Reads or wait the client to close the socket
			buf[bufFull]='\0';//Terminates the HTTP request string
			
			//printf("--------\nSize:%d\n%s\n",bufFull,buf);
			
			//True if the request is a post
			post=(strncmp(buf,"POST",4)==0);
			
			if (post) {//Joins POST done in 2 reads
				char* end=strstr(buf,"Content-Length");
				
				if (end==NULL) {
					//Get POST data with a 2nd read
					bufFull+=read(sock, buf+bufFull,INBUFFER-bufFull);
					buf[bufFull]='\0';
					//printf("--------CORRECTED\nSize:%d\n%s\n",bufFull,buf);
				}
				
			}
			
			req=strtok_r(buf," ",&lasts);
				
			//If the request is a get or a post
			if (!strcasecmp("get",req) || !strcasecmp("post",req)) {
				page=strtok_r(NULL," ",&lasts);
				syslog(LOG_INFO,"Requested page: %s to Thread %ld",page,id);
				
				//Stores the parameters of the request
				param=(char *)(page+strlen(page)+1);
				
				
				//request_auth(sock,"Autorizzazione");
				
				if (sendPage(sock,page,param,req)<0){ //Invia la pagina
					break;//Verificato errore nell'invio di un errore.
				}
			} else {//Non supported request
				send_err(sock,400,"Bad request");
				break; //Exits from the cycle and then close the connection.
			}
		}
				
		syslog(LOG_DEBUG,"Thread %ld: Closing socket with client",id);
		close(sock);//Closing the socket
		free_thread(id);//Settin this thread as free
	}
	
	return NULL;//Never reached
}

/**
This function does some changes on the URL.
*/
void modURL(char* url) {
	//Prevents the use of .. to access the whole filesystem
	strReplace(url,"../",'\0');
	
	
	{//Replaces escape sequences
		char e_seq[3];
		e_seq[2]=0;
		
		int i=0;		
		//Parses the string
		while (url[i]!=0) {
			
			//Search for escape char %
			if (url[i]=='%') {
				
				//Puts hex value in the buffer
				e_seq[0]=url[i+1];
				e_seq[1]=url[i+2];
						
				delChar(url,i,2);//Deletes 2 chars from the url
				
				//Replaces the 3rd character with the char corresponding to the escape
				url[i]=strtol( e_seq , NULL, 16);
			}
			i++;
		}
	}
	
	//TODO AbsoluteURI: Check if the url uses absolute url, and in that case remove the 1st part
}

/**
Questo metodo determina la pagina richiesta dal client e la invia
http_param is a string containing parameters of the HTTP request
*/
int sendPage(int sock,char * page,char * http_param,char * method) {
	
	
	modURL(page);//Operations on the url string
	
	char * params=NULL;//Puntatore alla stringa che contiene i parametri
	int p_start=nullParams(page);
	if (p_start!=-1) params=page+p_start+sizeof(char);//Faccio puntare ai parametri
	
	
	syslog (LOG_DEBUG,"URL changed into %s",page);
	
	int retval;//Return value after sending the page
	
	if (estensione(page,".php")==0) {//File php
		retval= execPage(sock,page,params,"php",http_param,method);
	} else if (estensione(page,".bsh")==0) {//Script bash
		retval=execPage(sock,page,params,"bash",http_param,method);
	} else {//Normal file
		retval= writePage(sock,page);
	}
	
	int err_res;
	
	if (retval==0) return 0;//No error
	else if (retval==ERR_BRKPIPE) err_res = send_err(sock,500,"Internal server error");
	else if (retval==ERR_FILENOTFOUND) err_res = send_err(sock,404,"Page not found");
	else if (retval==ERR_NOMEM) err_res = send_err(sock,503,"Service Unavailable");
	
	return err_res;
}
/**
Executes a script with a given interpreter and sends the resulting output
*/
int execPage(int sock, char * file, char * params,char * executor,char * http_param,char * method) {
	
	char * strfile=malloc(INBUFFER+strlen(basedir));//Buffer for file's name
	
	if (strfile==NULL)return ERR_NOMEM;//Returns if buffer was not allocated
	
	//Creo la stringa con il nome del file
	sprintf(strfile,"%s%s",basedir,file);//Compone la stringa con il path e il nome del file
	syslog(LOG_INFO,"Executing file %s",strfile);
	
	//Checks if the requested file exists
	if (file_exists(strfile)!=0) return ERR_FILENOTFOUND;
	
	int wpid;//Child's pid
	int wpipe[2];//Pipe's file descriptor
	
	pipe(wpipe);//Pipe to comunicate with the child
	wpid=fork();
	if (wpid<0) {//Error, returns a no memory error
		syslog(LOG_ERR,"Unable to fork to execute the file %s",strfile);
		return ERR_NOMEM;
	}
	if (wpid==0) { //Child, executes the script
		
		
		
		setEnvVars(http_param,method);
		
		close (wpipe[0]); //Closes unused end of the pipe
		fclose (stdout); //Closing the stdout
		fclose (stderr);
		dup(wpipe[1]); //Redirects the stdout
		if (params!=NULL) { 
			/*
			int args= splitParams(params);//Trasforma i parametri in tante stringhe
			char* prms[args+2];
			{//Fa puntare alle varie stringhe
				int in;
				prms[0]=executor;//Points to the program name
				
				printf("%s\n",prms[0]);

				
				char* next=params;
				for (in=1;in<=args;in++) {
					prms[in]=next;
					next=findNext(next);
					
					printf("%s\n",prms[in]);
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
	} else { //Reads from pipe and sends
		
		int state;
		waitpid (wpid,&state,0); //Wait the termination of the script
		
		//if (state==0 || state!=0) {
			//Large buffer, must contain the output of the script
			char* scrpt_buf=malloc(MAXSCRIPTOUT);
			
			if (scrpt_buf==NULL) {//Was unable to allocate the buffer
				free(strfile);
				return ERR_NOMEM;//Returns if buffer was not allocated
			}
		
			int reads=read(wpipe[0],scrpt_buf,MAXSCRIPTOUT);
			int wrote=0;
			send_http_header(sock,reads);
			wrote=write (sock,scrpt_buf,reads);
			if (wrote<0) syslog(LOG_ERR,"The client closed the socket");
			free(scrpt_buf);
		/*} else {
			return ERR_BRKPIPE;//L'interprete è fallito. Internal server error
		}*/
	}
	free(strfile);
	return 0;
}

/**
Questa funzione scrive sulla socket una pagina html contenente l'elenco dei file
all'interno della directory specificata
*/
int writeDir(int sock, char* page) {
	
	char* html=malloc(MAXSCRIPTOUT);//Memory for the html page
	if (html==NULL) {//No memory
		return ERR_NOMEM;
	}
	
	/*
	Determines if has to show the link to parent dir or not.
	If page is the basedir, it won't show the link to ..
	*/
	bool parent=true;
	{
		size_t page_len=strlen(page);
		size_t basedir_len=strlen(basedir);
		if (page_len-1==basedir_len || page_len==basedir_len) parent=false;
	}
	
	
	if (list_dir (page,html,MAXSCRIPTOUT,parent)<0) {//Creates the page
		return ERR_FILENOTFOUND;
	} else {//If there are no errors sends the page
		int pagelen=strlen(html);
		send_http_header(sock,pagelen);
		write(sock,html,pagelen);
	}
	
	free(html);//Frees the memory used for the page
	
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
				int dires=writeDir(sock,dir);
				free(strfile);
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
Sends a request for authentication 
*/
int request_auth(int sock,char * descr) {
	
	//Buffer for both header and page
	char * head=malloc(MAXSCRIPTOUT+HEADBUF);	
	if (head==NULL) return ERR_NOMEM;
	
	char * page=head+HEADBUF;
	
	//Prepares html page
	int page_len=sprintf(page,"%s<H1>Authorization required</H1><p>%s</p>%s",HTMLHEAD,descr,HTMLFOOT);
	
	//Prepares http header
	int head_len = sprintf(head,"HTTP/1.1 401 Authorization Required: Weborf (GNU/Linux)\r\nContent-Length: %d\r\nWWW-Authenticate: Basic realm=\"%s\"\r\n\r\n",page_len,descr);
	
	//Sends the header
	if (write (sock,head,head_len)!=head_len) {
		free(head);
		return -4;
	}
	
	//Sends the page
	if (write(sock,page,page_len)!=page_len) {
		free(head);
		return -4;
	}
	
	free(head);
	return 0;
}


/**
Invia un errore al client
*/
int send_err(int sock,int err,char* descr) {
	
	//Buffer for both header and page
	char * head=malloc(MAXSCRIPTOUT+HEADBUF);
	
	if (head==NULL) return ERR_NOMEM;
	
	//
	char * page=head+HEADBUF;
	
	//Prepares the page
	int page_len=sprintf(page,"%s <H1>Error %d</H1>%s %s",HTMLHEAD,err,descr,HTMLFOOT);
	
	//Prepares the header
	int head_len = sprintf(head,"HTTP/1.1 %d %s: Weborf (GNU/Linux)\r\nContent-Length: %d\r\n\r\n",err,descr ,(int)page_len);
	
	//Sends the http header
	if (write (sock,head,head_len)!=head_len) {
		free(head);
		return -4;
	}
	
	//Sends the html page
	if (write(sock,page,page_len)!=page_len) {
		free(head);
		return -4;
	}
	
	syslog(LOG_ERR,"Error %d: %s",err,descr);
	
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
	
	int len_head=sprintf(head,"HTTP/1.1 200 OK Server: Weborf (GNU/Linux)\r\nContent-Length: %d\r\n\r\n", size);	
	
	int wrote=write (sock,head,len_head);
	free(head);
	if (wrote!=len_head) return ERR_BRKPIPE;//Problema del server
	return 0;
}

