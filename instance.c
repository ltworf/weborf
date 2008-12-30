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

extern char* indexes[MAXINDEXCOUNT];
extern int indexes_l;

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

    if (authbin!=NULL && check_auth(sock,http_param, method, page, ip_addr)!=0) { //If auth is required
        return ERR_NONAUTH;
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


    int strfile_l=strlen(page)+strlen(basedir)+INDEXMAXLEN+1;
    char * strfile=malloc(strfile_l);//buffer for filename
    if (strfile==NULL)return ERR_NOMEM;//If no memory is available
    int strfile_e = snprintf(strfile,strfile_l,"%s%s",basedir,page);//Prepares the string
    int retval=0;//Return value after sending the page

    int f_mode=fileIsA(strfile);//Get file's mode
    if (S_ISDIR(f_mode)) {//Requested a directory
        bool index_found=false;

        if (!endsWith(strfile,"/")) {//Putting the ending / and redirect
            char* head=malloc(strfile_l+12);//12 is the size for the location header
            snprintf(head,strfile_l+12,"Location: %s/\r\n",page);
            send_http_header_code(sock,303,0,head);
            free(head);
        } else {

            char* index_name=&strfile[strfile_e];//Pointer to where to write the filename
            int i;

            //Cyclyng through the indexes
            for (i=0; i<indexes_l;i++) {
                snprintf(index_name,INDEXMAXLEN,"%s",indexes[i]);//Add INDEX to the url
                if (file_exists(strfile)) { //If index exists, redirect to it
                    char* head=malloc(strfile_l+12);//12 is the size for the location header
                    snprintf(head,strfile_l+12,"Location: %s%s\r\n",page,indexes[i]);
                    send_http_header_code(sock,303,0,head);
                    free(head);
                    index_found=true;
                    break;
                }
            }

            strfile[strfile_e]=0; //Removing the index part
            if (index_found==false) {//If no index was found in the dir
                writeDir(sock,strfile);
            }
        }
    } else if (file_exists(strfile)) {//Requested an existing file

        if (exec_script) { //Scripts enabled
            if (endsWith(page,".php")) { //Script php
                retval=execPage(sock,page,strfile,params,CGI_WRAPPER,http_param,post_param,method,ip_addr);
            } else { //Normal file
                retval= writePage(sock,strfile);
            }
        } else { //Scripts disabled
            retval= writePage(sock,strfile);
        }
    } else {//File doesn't exist
        retval=ERR_FILENOTFOUND;
    }

    if (post_param!=NULL) free (post_param);
    free(strfile);


    switch (retval) {
    case 0:
        return 0;
    case ERR_BRKPIPE:
        return send_err(sock,500,"Internal server error",ip_addr);
    case ERR_FILENOTFOUND:
        return send_err(sock,404,"Page not found",ip_addr);
    case ERR_NOMEM:
        return send_err(sock,503,"Service Unavailable",ip_addr);
    }
    return 0; //Make gcc happy
}
/**
Executes a script with a given interpreter and sends the resulting output
*/
int execPage(int sock, char * file,char* strfile, char * params,char * executor,char * http_param,char* post_param,char * method,char* ip_addr) {
#ifdef SENDINGDBG
    syslog(LOG_INFO,"Executing file %s",strfile);
#endif

    int wpid;//Child's pid
    int wpipe[2];//Pipe's file descriptor
    int ipipe[2];//Pipe's file descriptor, used to pass POST on script's standard input

    pipe(wpipe);//Pipe to comunicate with the child

    if (post_param!=NULL) {//Pipe created and used only if there is data to send to the script
        //Send post data to script's stdin
        pipe(ipipe);//Pipe to comunicate with the child
        write(ipipe[1],post_param,strlen(post_param));
        close (ipipe[1]); //Closes unused end of the pipe
    }

    wpid=fork();
    if (wpid<0) { //Error, returns a no memory error
#ifdef SENDINGDBG
        syslog(LOG_ERR,"Unable to fork to execute the file %s",strfile);
#endif
        if (post_param!=NULL) {
            close(ipipe[0]);
        }
        close(wpipe[1]);
        close(wpipe[0]);
        return ERR_NOMEM;
    } else if (wpid==0) { //Child, executes the script

        close (wpipe[0]); //Closes unused end of the pipe

        fclose (stdout); //Closing the stdout
        fclose (stderr);
        dup(wpipe[1]); //Redirects the stdout
        dup(wpipe[1]); //Redirects the stderr

        //Redirecting standard input
        if (post_param!=NULL) {//Send post data to script's stdin
            fclose(stdin);
            dup(ipipe[0]);
        }

        {//Clears all the env var, saving only SERVER_PORT
            char*port=getenv("SERVER_PORT");
            clearenv();
            setenv("SERVER_PORT",port,true);
        }
        setEnvVars(http_param); //Sets env var starting with HTTP
        setIpEnv(); //Sets SERVER_ADDR var
        //Set CGI needed vars
        setenv("SERVER_SIGNATURE",SIGNATURE,true);
        setenv("SERVER_SOFTWARE",SIGNATURE,true);
        setenv("GATEWAY_INTERFACE","CGI/1.1",true);
        setenv("REQUEST_METHOD",method,true); //POST GET
        setenv("SERVER_NAME", getenv("HTTP_HOST"),true);
        setenv("REDIRECT_STATUS","Ciao",true); // Mah.. i'll never understand php, this env var is needed
        setenv("SCRIPT_FILENAME",strfile,true); //This var is needed as well or php say no input file...
        //#env['DOCUMENT_ROOT']=sys.argv[6]

        setenv("REMOTE_ADDR",ip_addr,true); //#Client's address
        setenv("SCRIPT_NAME",file,true); //Name of the script without complete path

        //Request URI with or without a query
        if (params==NULL) {
            setenv("REQUEST_URI",file,true);
            setenv("QUERY_STRING","",true);//Query after ?
        } else {
            setenv("QUERY_STRING",params,true);//Query after ?

            //file and params were the same string.
            //Joining them again temporarily
            int delim=strlen(file);
            file[delim]='?';
            setenv("REQUEST_URI",file,true);
            file[delim]='\0';
        }

        {//If Content-Length field exists
            char *content_l=getenv("HTTP_CONTENT_LENGTH");
            if (content_l!=NULL) {
                setenv("CONTENT_LENGTH",content_l,true);
                setenv("CONTENT_TYPE",getenv("HTTP_CONTENT_TYPE"),true);
            }
        }

        alarm(SCRPT_TIMEOUT);//Sets the timeout for the script
        execl(executor,executor,(char *)0);
#ifdef SENDINGDBG
        syslog(LOG_ERR,"Execution of the %s interpreter failed",executor);
        perror("Execution of the page failed");
#endif
        exit(1);

    } else { //Father: reads from pipe and sends

        //Closing pipes, so if they're empty read is non blocking
        close (wpipe[1]);

        if (post_param!=NULL) {
            close(ipipe[0]);
        }

        //Large buffer, must contain the output of the script
        char* header_buf=malloc(MAXSCRIPTOUT+HEADBUF);

        if (header_buf==NULL) { //Was unable to allocate the buffer
            close (wpipe[0]);
            return ERR_NOMEM;//Returns if buffer was not allocated
        }

        {
            int state;
            waitpid (wpid,&state,0); //Wait the termination of the script
        }

        //Reads output of the script
        int e_reads=read(wpipe[0],header_buf,MAXSCRIPTOUT+HEADBUF);

        //Separating header from contents
        char* scrpt_buf=strstr(header_buf,"\r\n\r\n");
        int reads=0;
        if (scrpt_buf!=NULL) {
            scrpt_buf+=2;
            scrpt_buf[0]=0;
            scrpt_buf=scrpt_buf+2;
            reads=e_reads - (int)(scrpt_buf-header_buf);//Len of the content
        } else {//Something went wrong, ignoring the output (it's missing the headers)
            e_reads=0;
        }

        //Closing pipe
        close (wpipe[0]);

        if (e_reads>0) {//There is output from script
            char* status="200"; //Standard status
            {//Reading if there is another status
                char*s=strstr(header_buf,"Status: ");
                if (s!=NULL) {
                    status=s+8; //Replacing status
                }
            }
            send_http_header_scode(sock,status,reads,header_buf);


            if (reads>0) {//Sends the page if there is something to send
                write (sock,scrpt_buf,reads);
            }
        } else {//No output from script, maybe terminated...
            send_err(sock,500,"Internal server error",ip_addr);
        }

        free(header_buf);

    }
    return 0;
}

/**
This function writes on the specified socket an html page containing the list of files within the
specified directory.
*/
int writeDir(int sock, char* page) {
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

    char* html=malloc(MAXSCRIPTOUT);//Memory for the html page
    if (html==NULL) { //No memory
        return ERR_NOMEM;
    }


    if (list_dir (page,html,MAXSCRIPTOUT,parent)<0) { //Creates the page
        free(html);//Frees the memory used for the page
        return ERR_FILENOTFOUND;
    } else { //If there are no errors sends the page
        int pagelen=strlen(html);
        send_http_header(sock,pagelen,NULL);
        write(sock,html,pagelen);
    }

    free(html);//Frees the memory used for the page

    return 0;
}

/**
This function reads a file and writes it to the socket.
Also sends the http header with the content length header
same as the file's size.
There is no limit to file size, and it uses O_LARGEFILE flag
to send file larger than 4gb. Since this flag isn't available on
all systems, it will be 0 when compiled on system who doesn't have
this flag. On those systems it is unpredictable if it will be able to
send large files or not.
*/
int writePage(int sock,char * strfile) {

    int fp=open(strfile,O_RDONLY | O_LARGEFILE);
    if (fp<0) { //open returned an error
        return ERR_FILENOTFOUND;
    }

    char* buf=malloc(FILEBUF);//Buffer to read from file
    if (buf==NULL) {
        return ERR_NOMEM;//If no memory is available
    }

    {
        unsigned int size;//File's size
        {//Gets file's size
            struct stat buf;
            fstat(fp, &buf);
            //Ignoring errors, usually are due to large files, but the size is correctly returned anyway
            size=buf.st_size;
        }

        send_http_header(sock,size,NULL);//Sends header with content length
    }


    int reads;
    int wrote;
    
    //Sends file
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
    int page_len=snprintf(page,MAXSCRIPTOUT,"%s<H1>Authorization required</H1><p>%s</p>%s",HTMLHEAD,descr,HTMLFOOT);

    //Prepares http header
    int head_len = snprintf(head,HEADBUF,"HTTP/1.1 401 Authorization Required: Weborf (GNU/Linux)\r\nContent-Length: %d\r\nWWW-Authenticate: Basic realm=\"%s\"\r\n\r\n",page_len,descr);

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
    int page_len=snprintf(page,MAXSCRIPTOUT,"%s <H1>Error %d</H1>%s %s",HTMLHEAD,err,descr,HTMLFOOT);

    //Prepares the header
    int head_len = snprintf(head,HEADBUF,"HTTP/1.1 %d %s: Weborf (GNU/Linux)\r\nContent-Length: %d\r\nContent-Type: text/html\r\n\r\n",err,descr ,(int)page_len);

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
This function sends a code header to the specified socket
size is the Content-Length field.
headers can be NULL or some extra headers to add. Headers must be
separated by \r\n and must have an \r\n at the end.
Code is a string, no need to terminate it with a \0 because only the 1st 2nd and 3rd chars of it
will be used in each case. A shorter string will produce unpredictable behaviour
*/
int send_http_header_scode(int sock,char* code, unsigned int size,char* headers) {

    char *head=malloc(HEADBUF);
    if (head==NULL)return ERR_NOMEM;

    int len_head;
    if (headers==NULL) {
        len_head=snprintf(head,HEADBUF,"HTTP/1.1 %c%c%c OK Server: Weborf (GNU/Linux)\r\nContent-Length: %u\r\n\r\n",code[0],code[1],code[2], size);
    } else {
        len_head=snprintf(head,HEADBUF,"HTTP/1.1 %c%c%c OK Server: Weborf (GNU/Linux)\r\nContent-Length: %u\r\n%s\r\n",code[0],code[1],code[2], size,headers);
    }
    int wrote=write (sock,head,len_head);
    free(head);
    if (wrote!=len_head) return ERR_BRKPIPE;
    return 0;
}

/**
This function sends a code header to the specified socket
size is the Content-Length field.
headers can be NULL or some extra headers to add. Headers must be
separated by \r\n and must have an \r\n at the end.
*/
int send_http_header_code(int sock,int code, unsigned int size,char* headers) {

    char *head=malloc(HEADBUF);
    if (head==NULL)return ERR_NOMEM;

    int len_head;
    if (headers==NULL) {
        len_head=snprintf(head,HEADBUF,"HTTP/1.1 %d OK Server: Weborf (GNU/Linux)\r\nContent-Length: %u\r\n\r\n",code, size);
    } else {
        len_head=snprintf(head,HEADBUF,"HTTP/1.1 %d OK Server: Weborf (GNU/Linux)\r\nContent-Length: %u\r\n%s\r\n",code, size,headers);
    }
    int wrote=write (sock,head,len_head);
    free(head);
    if (wrote!=len_head) return ERR_BRKPIPE;
    return 0;
}

/**
This function sends a code 200 header to the specified socket
size is the Content-Length field
*/
int send_http_header(int sock,unsigned int size,char* headers) {
    return send_http_header_code(sock,200,size,headers);
}


/**
This function checks if the authentication can be granted or not calling the external program.
Returns 0 if authorization is granted.
*/
int check_auth(int sock, char* http_param, char * method, char * page, char * ip_addr) {
    //Buffer for username and password
    char* auths=malloc(INBUFFER+(2*PWDLIMIT));
    if (auths==NULL) return ERR_NOMEM;

    char username[PWDLIMIT*2];
    char* password=username; //will be changed if there is a password

    char* auth=strstr(http_param,"Authorization: Basic ");//Locates the auth information
    if (auth==NULL) { //No auth informations
        username[0]=0;
        //password[0]=0;
    } else { //Retrieves provided username and password
        char*auth_end=strstr(auth,"\r\n");//Finds the end of the header
        char a[PWDLIMIT*2];
        auth+=21;//Moves the begin of the string to exclude Authorization: Basic
        if ((auth_end-auth+1)<(PWDLIMIT*2))
            memcpy(&a,auth,auth_end-auth); //Copies the base64 encoded string to a temp buffer
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

    snprintf(auths,INBUFFER+(2*PWDLIMIT),"%s %s \"%s\" %s \"%s\" \"%s\"",authbin,method,page,ip_addr,username,password);

    int result=system(auths);
    free(auths);
    if (result!=0) { //Failed
        request_auth(sock,page);//Sends a request for authentication
    }

    return result;

}
