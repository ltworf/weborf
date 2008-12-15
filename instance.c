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
@author Salvo Rinaldi <salvin@bluebottle.com>
 */
#include "instance.h"
#include "queue.h"
#include "options.h"
#include "mystring.h"
#include "utils.h"
#include "base64.h"
#include <string.h>

extern syn_queue_t queue; //Queue for open sockets

extern pthread_mutex_t m_free;//Mutex to modify t_free
extern unsigned int t_free;//free threads

extern char* basedir;//Basedir
extern char* authbin;//Executable that will authenticate
extern bool exec_script; //Execute scripts if true, sends the file if false

/**
Set thread with id as non-free
*/
void unfree_thread(long int id) {
    pthread_mutex_lock(&m_free);
    t_free--;
#ifdef THREADDBG
    syslog(LOG_DEBUG,"There are %d free threads",t_free);
#endif
    pthread_mutex_unlock(&m_free);
}

/**
Set thread with id as free
*/
void free_thread(long int id) {
    pthread_mutex_lock(&m_free);
    t_free++;
#ifdef THREADDBG
    syslog(LOG_DEBUG,"There are %d free threads",t_free);
#endif
    pthread_mutex_unlock(&m_free);
}

/**
Function executed at the beginning of the thread
Takes open sockets from the queue and serves the requests
Doesn't do busy waiting
*/
void * instance(void * nulla) {
    //General init of the thread
    signal(SIGPIPE, SIG_IGN);//Ignores SIGPIPE
    long int id=(long int)nulla;//Set thread's id
#ifdef THREADDBG
    syslog(LOG_DEBUG,"Starting thread %ld",id);
#endif

    //Vars
    bool keep_alive;//True if we are using pipelining
    int bufFull=0;//Amount of buf used
    char * buf=malloc(INBUFFER+1);//Buffer to contain the HTTP request
    memset(buf,0,INBUFFER+1);//Sets to 0 the buffer
    int req;//Method of the HTTP request INTEGER
    char * reqs;//HTTP request STRING
    char * page;//Page to load
    char * lasts;//Used by strtok_r

    char * param;//HTTP parameter

    int sock;//Socket with the client
    char* ip_addr;//Client's ip address in ascii
    while (true) {

        q_get(&queue, &sock,&ip_addr);//Gets a socket from the queue
        unfree_thread(id);//Sets this thread as busy

        if (sock<0) { //Was not a socket but a termination order
#ifdef THREADDBG
            syslog(LOG_DEBUG,"Terminating thread %ld",id);
#endif
            free(buf);
            pthread_exit(0);
        }

#ifdef THREADDBG
        syslog(LOG_DEBUG,"Thread %ld: Reading from socket",id);
#endif


        while (true) { //Infinite cycle to handle all pipelined requests

            memset(buf,0,bufFull+1);//Sets to 0 the buffer, only the part used for the previous request in the same connection
            bufFull=0;//bufFull-(end-buf+4);

            int r;//Readed char
            char* end;//Pointer to header's end
            while ((end=strstr(buf,"\r\n\r\n"))==NULL) { //Determines if there is a double \r\n
                r=read(sock, buf+bufFull,1);//Reads 1 char and adds to the buffer

                if (r<=0) { //Connection closed or error
                    goto closeConnection;
                }

                if (bufFull!=0) { //Removes Cr Lf from beginning
                    bufFull+=r;//Sets the end of the user buffer (may contain more than one header)

                } else if (buf[bufFull]!='\n' && buf[bufFull]!='\r') {
                    bufFull+=r;
                }


                if (bufFull>=INBUFFER) { //Buffer full and still no valid http header
                    send_err(sock,400,"Bad request",ip_addr);
                    goto closeConnection;
                }
            }

            end[2]='\0';//Terminates the header, leaving a final \r\n in it

            //Removes cr and lf chars from the beginning
            //removeCrLf(buf);

            //Finds out request's kind
            if (strncmp(buf,"GET",3)==0) req=GET;
            else if (strncmp(buf,"POST",4)==0) req=POST;
            else req=INVALID;

            if ( req!=INVALID ) {
                reqs=strtok_r(buf," ",&lasts);//Must be done to eliminate the request
                page=strtok_r(NULL," ",&lasts);

#ifdef REQUESTDBG
                syslog(LOG_INFO,"%s: %s %s\n",ip_addr,reqs,page);
#endif

#ifdef THREADDBG
                syslog(LOG_INFO,"Requested page: %s to Thread %ld",page,id);
#endif
                //Stores the parameters of the request
                param=(char *)(page+strlen(page)+1);

                //Setting the connection type, using protocol version
                if (param[5]=='1' && param[7]=='1') {//Keep alive by default
                    keep_alive=true;
                    char a[50];//Gets the value
                    if (get_param_value(param,"Connection", a,50))
                        if (strncmp (a,"close",5)==0)
                            keep_alive=false;
                } else {
                    keep_alive=false;
                    char a[50];//Gets the value
                    if (get_param_value(param,"Connection", a,50))
                        if (strncmp(a,"Keep",4)==0)
                            keep_alive=true;
                }

                if (sendPage(sock,page,param,req,reqs,ip_addr)<0) {
                    break;//Unable to send an error
                }
                if (keep_alive==false) {//No pipelining

                    goto closeConnection;
                }
            } else { //Non supported request
                send_err(sock,400,"Bad request",ip_addr);
                goto closeConnection;//Exits from the cycle and then close the connection.
            }

            //Deletes the served header and moves the following part of the buffer at the beginning
            //memmove(buf,end+4,bufFull-(end-buf+4));
        }

closeConnection:
#ifdef THREADDBG
        syslog(LOG_DEBUG,"Thread %ld: Closing socket with client",id);
#endif

        close(sock);//Closing the socket
        memset(buf,0,bufFull+1);//Sets to 0 the buffer, only the part used for the previous
        if (ip_addr!=NULL) free(ip_addr);//Free the space used to store ip address
        free_thread(id);//Settin this thread as free
    }

    return NULL;//Never reached
}

/**
This function does some changes on the URL.
The url will never be longer than the original one.
*/
void modURL(char* url) {
    //Prevents the use of .. to access the whole filesystem
    strReplace(url,"../",'\0');

    replaceEscape(url);

    //TODO AbsoluteURI: Check if the url uses absolute url, and in that case remove the 1st part
}

/**
This function determines the requested page and sends it
http_param is a string containing parameters of the HTTP request
*/
int sendPage(int sock,char * page,char * http_param,int method_id,char * method,char* ip_addr) {

    modURL(page);//Operations on the url string


    char * params=NULL;//Pointer to the GET parameters
    int p_start=nullParams(page);
    if (p_start!=-1) params=page+p_start+sizeof(char);//Set the pointer to the parameters
#ifdef SENDINGDBG
    syslog (LOG_DEBUG,"URL changed into %s",page);
#endif
    if (authbin!=NULL) { //If auth is required
        char* auths=malloc(INBUFFER+(2*PWDLIMIT));
        if (auths==NULL) return ERR_NOMEM;

        char username[PWDLIMIT*2];
        char* password=username+PWDLIMIT;

        char* auth=strstr(http_param,"Authorization: Basic ");//Locates the auth information
        if (auth==NULL) { //No auth informations
            username[0]=0;
            password[0]=0;
        } else { //Retrieves provided username and password
            char*auth_end=strstr(auth,"\r\n");//
            char a[PWDLIMIT*2];
            auth+=21;//Moves the begin of the string to exclude Authorization: Basic
            if ((auth_end-auth+1)<(PWDLIMIT*2))
                memcpy(&a,auth,auth_end-auth);
            else { //Auth string is too long for the buffer
#ifdef SERVERDBG
                syslog(LOG_ERR,"Unable to accept authentication, buffer is too small");
#endif
                free(auths);
                return ERR_NOMEM;
            }

            a[auth_end-auth]=0;
            decode64(username,a);//Decodes the base64 string

            password=strstr(username,":");//Locates the separator :
            password[0]=0;//Nulls the separator to split username and password
            password++;//make password point to the beginning of the password
        }

        sprintf(auths,"%s %s \"%s\" %s \"%s\" \"%s\"",authbin,method,page,ip_addr,username,password);
        if (system(auths)!=0) { //Failed
            free(auths);
            return request_auth(sock,page);//Sends a request for authentication
        }

        free(auths);
    }

    char* post_param=NULL;

    {
        //Buffer for field's value
        char a[NBUFFER];
        //Gets the value
        bool r=get_param_value(http_param,"Content-Length", a,NBUFFER);

        //If there is a value and method is POST
        if (r!=false && method_id==POST) {
            int l=strtol( a , NULL, 0 );
            if (l<=POST_MAX_SIZE) {//Post size is ok
                post_param=malloc(l+20);

                int count=read(sock,post_param,l);
                post_param[count]=0;
                int removed=removeCrLf(post_param);
                read(sock,post_param+count-removed,removed);
                post_param[count+removed]=0;

            } else {//Post size is too big
                return ERR_NOMEM;
            }
        }

    }

    int retval;//Return value after sending the page

    if (exec_script) { //Scripts enabled
        if (endsWith(page,".php")) { //File php
            retval= execPage(sock,page,params,"php",http_param,post_param,method);
        } else if (endsWith(page,".bsh")) { //Script bash
            retval=execPage(sock,page,params,"bash",http_param,post_param,method);
        } else if (endsWith(page,".py")) { //Script bash
            retval=execPage(sock,page,params,"pywrap.py",http_param,post_param,method);
        } else { //Normal file
            retval= writePage(sock,page);
        }
    } else { //Scripts disabled
        retval= writePage(sock,page);
    }



    if (post_param!=NULL) free (post_param);

    int err_res=0;//suppose no error

    switch (retval) {
    case 0:
        err_res=0;
        break;
    case ERR_BRKPIPE:
        err_res = send_err(sock,500,"Internal server error",ip_addr);
        break;
    case ERR_FILENOTFOUND:
        err_res = send_err(sock,404,"Page not found",ip_addr);
        break;
    case ERR_NOMEM:
        err_res = send_err(sock,503,"Service Unavailable",ip_addr);
        break;
    }

    return err_res;
}
/**
Executes a script with a given interpreter and sends the resulting output
*/
int execPage(int sock, char * file, char * params,char * executor,char * http_param,char* post_param,char * method) {

    char * strfile=malloc(INBUFFER+strlen(basedir));//Buffer for file's name

    if (strfile==NULL)return ERR_NOMEM;//Returns if buffer was not allocated

    //String with file's name
    sprintf(strfile,"%s%s",basedir,file);//Puts together path and filename
#ifdef SENDINGDBG
    syslog(LOG_INFO,"Executing file %s",strfile);
#endif

    //Checks if the requested file exists
    if (!file_exists(strfile)) {
        free(strfile);
        return ERR_FILENOTFOUND;
    }

    int wpid;//Child's pid
    int wpipe[2];//Pipe's file descriptor
    int hpipe[2];//Pipe's file descriptor, used for extra headers

    pipe(wpipe);//Pipe to comunicate with the child
    pipe(hpipe);//Pipe to comunicate with the child
    wpid=fork();
    if (wpid<0) { //Error, returns a no memory error
#ifdef SENDINGDBG
        syslog(LOG_ERR,"Unable to fork to execute the file %s",strfile);
#endif
        free(strfile);
        return ERR_NOMEM;
    }
    if (wpid==0) { //Child, executes the script
        setenv("PROTOCOL",method,true);//Sets the protocol used
        setEnvVars(http_param,false);//Sets http header vars
        setEnvVars(post_param,true);//Sets post vars

        close (wpipe[0]); //Closes unused end of the pipe
        close (hpipe[0]); //Closes unused end of the pipe
        fclose (stdout); //Closing the stdout
        fclose (stderr);
        dup(wpipe[1]); //Redirects the stdout
        dup(wpipe[1]); //Redirects the stderr
        dup2(hpipe[1],4);//Pipe for extra headers

        alarm(SCRPT_TIMEOUT);//Sets the timeout for the script

        if (params!=NULL) {
            //Gives GET parameters
            execlp(executor,executor,strfile,params,(char *)0);
#ifdef SENDINGDBG
            syslog(LOG_ERR,"Execution of the %s interpreter failed",executor);
#endif

            exit(1);
        } else {
#ifdef SENDINGDBG
            syslog(LOG_DEBUG,"Executing %s interpreter for %s",executor,strfile);
#endif

            execlp(executor,executor,strfile,(char *)0);

#ifdef SENDINGDBG
            syslog(LOG_ERR,"Execution of the %s interpreter failed",executor);
#endif

            exit(1);
        }
    } else { //Reads from pipe and sends

        int state;
        waitpid (wpid,&state,0); //Wait the termination of the script

        //Large buffer, must contain the output of the script
        char* scrpt_buf=malloc(MAXSCRIPTOUT+HEADBUF);
        char* header_buf=scrpt_buf+MAXSCRIPTOUT;

        if (scrpt_buf==NULL) { //Was unable to allocate the buffer
            free(strfile);
            return ERR_NOMEM;//Returns if buffer was not allocated
        }
        
        //Closing pipes, so if they're empty read is non blocking
        close (wpipe[1]);
        close (hpipe[1]);
        int reads=read(wpipe[0],scrpt_buf,MAXSCRIPTOUT);
        printf("Reading headers\n");
        int h_reads=read(hpipe[0],header_buf,HEADBUF);
        printf("Read headers\n");
        
        //Closing pipes
        close (wpipe[0]);
        close (hpipe[0]);
        
        
        printf("%s\n",header_buf);
        
        int wrote=0;
        send_http_header(sock,reads);
        wrote=write (sock,scrpt_buf,reads);
#ifdef SOCKETDBG
        if (wrote<0) syslog(LOG_ERR,"The client closed the socket");
#endif
        free(scrpt_buf);
        /*} else {
        	return ERR_BRKPIPE;//Script returned a non 0. Internal server error
        }*/
    }
    free(strfile);
    return 0;
}

/**
This function writes on the specified socket an html page containing the list of files within the
specified directory.
*/
int writeDir(int sock, char* page) {

    char* html=malloc(MAXSCRIPTOUT);//Memory for the html page
    if (html==NULL) { //No memory
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


    if (list_dir (page,html,MAXSCRIPTOUT,parent)<0) { //Creates the page
        free(html);//Frees the memory used for the page
        return ERR_FILENOTFOUND;
    } else { //If there are no errors sends the page
        int pagelen=strlen(html);
        send_http_header(sock,pagelen);
        write(sock,html,pagelen);
    }

    free(html);//Frees the memory used for the page

    return 0;
}

/**
This function reads a file and writes it to the socket.
*/
int writePage(int sock,char * file) {
    char * strfile=malloc(strlen(file)+strlen(basedir)+1);//buffer for filename
    if (strfile==NULL)return ERR_NOMEM;//If no memory is available

    sprintf(strfile,"%s%s",basedir,file);//Prepares the string

    {//Check is it was requested a directory instead of a file
#ifdef SENDINGDBG
        syslog(LOG_DEBUG,"File: %s",strfile);
#endif

        int f_mode=fileIsA(strfile);//Get file's mode
        if (S_ISDIR(f_mode)) { //If it is a directory

            char* newpath=malloc(strlen(file)+strlen(INDEX)+2);
            if (newpath==NULL) {
                free(strfile);
                return ERR_NOMEM;
            }

            if (endsWith(strfile,"/")) {
                sprintf(newpath,"%s%s",file,INDEX);//Add INDEX to the url
            } else {
                sprintf(newpath,"%s/%s",file,INDEX);//Add INDEX to the url
            }

            int res=writePage(sock,newpath);//Sends the index
            free(newpath);

            if (res!=ERR_FILENOTFOUND) { //Index page not found
                free(strfile);
                return res;
            } else { //Creates a page with the list of files
                char* dir=strfile;
                int dires=writeDir(sock,dir);
                free(strfile);
                return dires;
            }
        }
    }

    char* buf=malloc(FILEBUF);

    if (buf==NULL) {
        free(strfile);
        return ERR_NOMEM;//If no memory is available
    }


    int fp=open(strfile,O_RDONLY | O_LARGEFILE);
    if (fp<0) { //open returned an error
        free((void *) strfile);
        free(buf);

        return ERR_FILENOTFOUND;
    }

    {
        unsigned int size;//File's size
        {//Gets file's size

            struct stat buf;
            fstat(fp, &buf);
            //Ignoring errors, usually are due to large files, but the size is correctly returned anyway
            size=buf.st_size;

        }

        send_http_header(sock,size);//Sends header with content length
    }

    int reads;
    int wrote;

    while ((reads=read(fp, buf, FILEBUF))>0) {
        wrote=write(sock,buf,reads);
        if (wrote!=reads) { //Error writing to the socket
#ifdef SOCKETDBG
            syslog(LOG_ERR,"Unable to send %s: error writing to the socket",file);
#endif
            break;
        }
    }

    close(fp);//File on filesystem

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
        return ERR_SOCKWRITE;
    }

    //Sends the page
    if (write(sock,page,page_len)!=page_len) {
        free(head);
        return ERR_SOCKWRITE;
    }

    free(head);
    return 0;
}


/**
Sends an error to the client
*/
int send_err(int sock,int err,char* descr,char* ip_addr) {

    //Buffer for both header and page
    char * head=malloc(MAXSCRIPTOUT+HEADBUF);

    if (head==NULL) return ERR_NOMEM;

    char * page=head+HEADBUF;

    //Prepares the page
    int page_len=sprintf(page,"%s <H1>Error %d</H1>%s %s",HTMLHEAD,err,descr,HTMLFOOT);

    //Prepares the header
    int head_len = sprintf(head,"HTTP/1.1 %d %s: Weborf (GNU/Linux)\r\nContent-Length: %d\r\n\r\n",err,descr ,(int)page_len);

    //Sends the http header
    if (write (sock,head,head_len)!=head_len) {
        free(head);
        return ERR_SOCKWRITE;
    }

    //Sends the html page
    if (write(sock,page,page_len)!=page_len) {
        free(head);
        return ERR_SOCKWRITE;
    }
#ifdef REQUESTDBG
    syslog(LOG_ERR,"%s: Error %d: %s",ip_addr,err,descr);
#endif

    free(head);
    return 0;
}

/**
This function sends a code 200 header to the specified socket
size is the Content-Length field
*/
int send_http_header(int sock,unsigned int size) {

    char *head=malloc(HEADBUF);
    if (head==NULL)return ERR_NOMEM;

    int len_head=sprintf(head,"HTTP/1.1 200 OK Server: Weborf (GNU/Linux)\r\nContent-Length: %u\r\n\r\n", size);

    int wrote=write (sock,head,len_head);
    free(head);
    if (wrote!=len_head) return ERR_BRKPIPE;
    return 0;
}

