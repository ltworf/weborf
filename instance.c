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
@author Salvo Rinaldi <salvin@anche.no>
 */
#include "instance.h"

extern syn_queue_t queue;                   //Queue for open sockets

extern t_thread_info thread_info;

extern char* basedir;                       //Basedir
extern char* authsock;                       //Executable that will authenticate
extern bool exec_script;                    //Execute scripts if true, sends the file if false

extern char* indexes[MAXINDEXCOUNT];        //Array containing index files
extern int indexes_l;                       //Length of array
extern bool virtual_host;                   //True if must check for virtual hosts
extern char ** environ;                     //To reset environ vars
extern array_ll cgi_paths;                  //Paths to cgi binaries

void handle_requests(int sock,char* buf,buffered_read_t * read_b,int * bufFull,connection_t* connection_prop,long int id) {
    int from;
    char * lasts;//Used by strtok_r

    short int r;//Readed char
    char* end;//Pointer to header's end

    while (true) { //Infinite cycle to handle all pipelined requests
        if ((*bufFull)!=0) {
            memset(buf,0,(*bufFull));//Sets to 0 the buffer, only the part used for the previous request in the same connection
            (*bufFull)=0;//bufFull-(end-buf+4);
        }
        from=0;

        while ((end=strstr(buf+from,"\r\n\r"))==NULL) { //Determines if there is a \r\n\r which is an ending sequence
            r=buffer_read(sock, buf+(*bufFull),2,read_b);//Reads 2 char and adds to the buffer

            if (r<=0) { //Connection closed or error
                return;
            }

            if (!(buf[*bufFull]==10 || buf[*bufFull]==13)) {//Optimization to make strstr parse only the ending part of the string
                from=(*bufFull);
            }

            if ((*bufFull)!=0) { //Removes Cr Lf from beginning
                (*bufFull)+=r;//Sets the end of the user buffer (may contain more than one header)
            } else if (buf[*bufFull]!='\n' && buf[*bufFull]!='\r') {
                (*bufFull)+=r;
            }

            if ((*bufFull)>=INBUFFER) { //Buffer full and still no valid http header
                send_err(sock,400,"Bad request",connection_prop->ip_addr);
                return;
            }
        }

        if (strstr(buf+from,"\r\n\r\n")==NULL) {//If we didn't read yet the lst \n of the ending sequence, we read it now, so it won't disturb the next request
            *bufFull+=buffer_read(sock, buf+*bufFull,1,read_b);//Reads 1 char and adds to the buffer
        }

        end[2]='\0';//Terminates the header, leaving a final \r\n in it

        //Finds out request's kind
        if (strncmp(buf,"GET",3)==0) connection_prop->method_id=GET;
        else if (strncmp(buf,"POST",4)==0) connection_prop->method_id=POST;
        else if (strncmp(buf,"PUT",3)==0) connection_prop->method_id=PUT;
        else if (strncmp(buf,"DELETE",6)==0) connection_prop->method_id=DELETE;
        else if (strncmp(buf,"OPTIONS",7)==0) connection_prop->method_id=OPTIONS;
#ifdef WEBDAV
        else if (strncmp(buf,"PROPFIND",8)==0) connection_prop->method_id=PROPFIND;
        else if (strncmp(buf,"MKCOL",5)==0) connection_prop->method_id=MKCOL;
        else if (strncmp(buf,"COPY",4)==0) connection_prop->method_id=COPY;
        else if (strncmp(buf,"MOVE",4)==0) connection_prop->method_id=MOVE;
#endif
        else {
            send_err(sock,400,"Bad request",connection_prop->ip_addr);
            return;
        }

        connection_prop->method=strtok_r(buf," ",&lasts);//Must be done to eliminate the request
        connection_prop->page=strtok_r(NULL," ",&lasts);
        connection_prop->http_param=lasts;

#ifdef REQUESTDBG
        //TODO for some strange reason sometimes there is a strange char after the ip addr, investigate why
        syslog(LOG_INFO,"%s - %s %s\n",connection_prop->ip_addr,connection_prop->method,connection_prop->page);
#endif

#ifdef THREADDBG
        syslog(LOG_INFO,"Requested page: %s to Thread %ld",connection_prop->page,id);
#endif
        //Stores the parameters of the request
        {
            char a[12];//Gets the value
            //Obtains the connection header, writing it into the a buffer, and sets connection=true if the header is present
            bool connection=get_param_value(connection_prop->http_param,"Connection", a,12,10);//12 is the buffer size and 10 is strlen("connection")

            //Setting the connection type, using protocol version
            if (connection_prop->http_param[7]=='1' && connection_prop->http_param[5]=='1') {//Keep alive by default (protocol 1.1)
                connection_prop->protocol_version=HTTP_1_1;
                connection_prop->keep_alive=(connection && strncmp(a,"close",5)==0)?false:true;
            } else {//Not http1.1
                //Constants are set to make this line work
                connection_prop->protocol_version=connection_prop->http_param[7];
                connection_prop->keep_alive=(connection && strncmp(a,"Keep",4)==0)?true:false;
            }
        }
        if (send_page(sock,read_b, connection_prop)<0) {
            return;//Unable to send an error
        }

        //Non pipelined
        if (connection_prop->keep_alive==false) return;

    } /* while */
}

/**
Set thread with id as free
*/
void change_free_thread(long int id,int free_d, int count_d) {
    pthread_mutex_lock(&thread_info.mutex);

    thread_info.free+=free_d;
    thread_info.count+=count_d;

#ifdef THREADDBG
    syslog(LOG_DEBUG,"There are %d free threads",thread_info.free);
#endif
    pthread_mutex_unlock(&thread_info.mutex);
}

/**
Function executed at the beginning of the thread
Takes open sockets from the queue and serves the requests
Doesn't do busy waiting
*/
void * instance(void * nulla) {
    //General init of the thread
    long int id=(long int)nulla;//Set thread's id
#ifdef THREADDBG
    syslog(LOG_DEBUG,"Starting thread %ld",id);
#endif

    //Vars
    int bufFull=0;                                  //Amount of buf used
    connection_t connection_prop;                   //Struct to contain properties of the connection
    buffered_read_t read_b;                         //Buffer for buffered reader
    int sock=0;                                     //Socket with the client
    char * buf=calloc(INBUFFER+1,sizeof(char));     //Buffer to contain the HTTP request

    signal(SIGPIPE, SIG_IGN);//Ignores SIGPIPE

#ifdef IPV6
    struct sockaddr_in6 addr;//Local and remote address
    socklen_t addr_l=sizeof(struct sockaddr_in6);
#else
    struct sockaddr_in addr;
    int addr_l=sizeof(struct sockaddr_in);
#endif

    if (buffer_init(&read_b,BUFFERED_READER_SIZE)!=0 || buf==NULL) { //Unable to allocate the buffer
        buffer_free(&read_b);             //Frees buffered reader if it was allocated or does nothing
        free(buf);                        //Frees buf if it was allocated or does nothing

#ifdef SERVERDBG
        syslog(LOG_CRIT,"Not enough memory to allocate buffers for new thread");
#endif
        change_free_thread(id,0,-1);
        pthread_exit(0);
    }

    //Start accepting sockets
    change_free_thread(id,1,0);

    while (true) {
        q_get(&queue, &sock);//Gets a socket from the queue
        change_free_thread(id,-1,0);//Sets this thread as busy

        if (sock<0) { //Was not a socket but a termination order
#ifdef THREADDBG
            syslog(LOG_DEBUG,"Terminating thread %ld",id);
#endif
            free(buf);
            buffer_free(&read_b);
            change_free_thread(id,0,-1);//Reduces count of threads
            pthread_exit(0);
        }

        //Converting address to string
#ifdef IPV6
        getpeername(sock, (struct sockaddr *)&addr, &addr_l);
        inet_ntop(AF_INET6, &addr.sin6_addr, connection_prop.ip_addr, INET6_ADDRSTRLEN);
#else
        getpeername(sock, (struct sockaddr *)&addr,(socklen_t *) &addr_l);
        inet_ntop(AF_INET, &addr.sin_addr, connection_prop.ip_addr, INET_ADDRSTRLEN);
#endif

#ifdef THREADDBG
        syslog(LOG_DEBUG,"Thread %ld: Reading from socket",id);
#endif
        handle_requests(sock,buf,&read_b,&bufFull,&connection_prop,id);

#ifdef THREADDBG
        syslog(LOG_DEBUG,"Thread %ld: Closing socket with client",id);
#endif

        close(sock);//Closing the socket
        //memset(buf,0,bufFull);//Sets to 0 the buffer, only the part used for the previous
        buffer_reset (&read_b);


        change_free_thread(id,1,0);//Sets this thread as free
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
This function handles a PUT request.

It requires the socket, connection_t struct and buffered_read_t to read from
the socket, since the reading is internally buffered.

PUT size has no hardcoded limits.
Auth provider has to check for the file's size and refuse it if it is the case.
This function will not work if there is no auth provider.
*/
int read_file(int sock,connection_t* connection_prop,buffered_read_t* read_b) {

    if (authsock==NULL) {
        return ERR_FORBIDDEN;
    }

    //Checking if there is any unsupported Content-* header. In this case return 501 (Not implemented)
    {
        char*header=connection_prop->http_param;

        while ((header=strstr(header,"Content-"))!=NULL) {
            if (strncmp(header,"Content-Length",14)!=0) {
                return ERR_NOTIMPLEMENTED;
            }
            header++;
        }
    }

    char a[NBUFFER]; //Buffer for field's value
    //Gets the value of content-length header
    bool r=get_param_value(connection_prop->http_param,"Content-Length", a,NBUFFER,14);//14 is content-lenght's len
    int content_l;  //Length of the put data

    //Checks if file already exists or not (needed for response code)
    bool preexistent=file_exists(connection_prop->strfile);

    if (r!=false) {//If there is no content-lenght returns error
        content_l=strtol( a , NULL, 0 );
    } else {//No data
        return ERR_NODATA;
    }

    int retval;

    if (preexistent) {//Resource already existed (No content)
        retval=OK_NOCONTENT;
    } else {//Resource was created (Created)
        retval=OK_CREATED;
    }

    int fd=open(connection_prop->strfile,O_WRONLY|O_CREAT,S_IRUSR|S_IWUSR);
    if (fd<0) {
        return ERR_FILENOTFOUND;
    }

    char* buf=malloc(FILEBUF);//Buffer to read from file
    if (buf==NULL) {
#ifdef SERVERDBG
        syslog(LOG_CRIT,"Not enough memory to allocate buffers");
#endif
        return ERR_NOMEM;
    }

    int read_,write_;
    int tot_read=0;
    int to_read;

    while ((to_read=(content_l-tot_read)>FILEBUF?FILEBUF:content_l-tot_read)>0) {
        read_=buffer_read(sock,buf,to_read,read_b);
        write_=write(fd,buf,read_);

        if (write_!=read_) {
            retval= ERR_BRKPIPE;
            break;
        }

        tot_read+=read_;
    }

    free(buf);
    close(fd);

    return retval;
}

/**
This function handles a DELETE request.

It requires the socket, connection_t struct

Auth provider has to check if it is allowed to delete the file or not.
This function will not work if there is no auth provider.
*/
int delete_file(int sock,connection_t* connection_prop) {
    int retval;

    if (authsock==NULL) {
        return ERR_FORBIDDEN;
    }

    //connection_prop->strfile
    struct stat stat_d;
    if (stat(connection_prop->strfile,&stat_d)<0) {
        return ERR_FILENOTFOUND;
    }

    if (S_ISDIR(stat_d.st_mode)) {
        retval=deep_rmdir(connection_prop->strfile);
    } else {
        retval=unlink(connection_prop->strfile);
    }

    if (retval!=0) {//Returns a generic error
        return ERR_FORBIDDEN;
    }
    return OK_NOCONTENT;
}

/**
Returns the list of supported methods. In theory this list should be
different depending on the URI requested. But this method will return
the same list for everything.
*/
int options (int sock, connection_t* connection_prop) {

#ifdef WEBDAV
#define ALLOWED "Allow: GET,POST,PUT,DELETE,OPTIONS,PROPFIND,MKCOL,COPY,MOVE\r\nDAV: 1,2\r\nDAV: <http://apache.org/dav/propset/fs/1>\r\nMS-Author-Via: DAV\r\n"
#else
#define ALLOWED "Allow: GET,POST,PUT,DELETE,OPTIONS\r\n"
#endif

    send_http_header_full(sock,200,0,ALLOWED,true,-1,connection_prop);
    return 0;
}

/**
This function determines the requested page and sends it
http_param is a string containing parameters of the HTTP request
*/
int send_page(int sock,buffered_read_t* read_b, connection_t* connection_prop) {

    int retval=0;//Return value after sending the page
    char* real_basedir; //Basedir, might be different than basedir due to virtual hosts
    string_t post_param; //Contains POST data

    modURL(connection_prop->page);//Operations on the url string
    split_get_params(connection_prop);//Splits URI into page and parameters

#ifdef SENDINGDBG
    syslog (LOG_DEBUG,"URL changed into %s",connection_prop->page);
#endif


    if (virtual_host) { //Using virtual hosts
        real_basedir=get_basedir(connection_prop->http_param);
    } else {//No virtual Host
        real_basedir=basedir;
    }

    if ((authsock!=NULL) && check_auth(sock,connection_prop)!=0) { //If auth is required
        retval = ERR_NOAUTH;
        post_param.data=NULL;
        connection_prop->strfile=NULL;

        goto escape;
    }

    connection_prop->strfile=malloc(URI_LEN);//buffer for filename
    if (connection_prop->strfile==NULL) {
#ifdef SERVERDBG
        syslog(LOG_CRIT,"Not enough memory to allocate buffers");
#endif
        return ERR_NOMEM;//If no memory is available
    }
    connection_prop->strfile_len = snprintf(connection_prop->strfile,URI_LEN,"%s%s",real_basedir,connection_prop->page);//Prepares the string

    if (connection_prop->method_id>=PUT) {//Methods from PUT to other uncommon ones :-D
        post_param.data=NULL;
        post_param.len=0;

        switch (connection_prop->method_id) {
        case PUT:
            retval=read_file(sock,connection_prop,read_b);
            break;
        case DELETE:
            retval=delete_file(sock,connection_prop);
            break;
        case OPTIONS:
            retval=options(sock,connection_prop);
            break;
#ifdef WEBDAV
        case PROPFIND:
            //Propfind has data, not strictly post but read_post_data will work
            post_param=read_post_data(sock,connection_prop,read_b);
            retval=propfind(sock,connection_prop,&post_param);
            break;
        case MKCOL:
            retval=mkcol(sock,connection_prop);
            break;
        case COPY:
        case MOVE:
            retval=copy_move(sock,connection_prop);
            break;
#endif
        }

        goto escape;
    }

    post_param=read_post_data(sock,connection_prop,read_b);

    //Opening file and doing stat
    if ((connection_prop->strfile_fd=open(connection_prop->strfile,O_RDONLY | O_LARGEFILE))<0) {
        //File doesn't exist. Must return errorcode
        retval=ERR_FILENOTFOUND;
        goto escape;
    }

    fstat(connection_prop->strfile_fd, &connection_prop->strfile_stat);

    if (S_ISDIR(connection_prop->strfile_stat.st_mode)) {//Requested a directory without ending /
        bool index_found=false;

        if (!endsWith(connection_prop->strfile,"/",connection_prop->strfile_len,1)) {//Putting the ending / and redirect
            char head[URI_LEN+12];//12 is the size for the location header
            snprintf(head,URI_LEN+12,"Location: %s/\r\n",connection_prop->page);
            send_http_header_full(sock,301,0,head,true,-1,connection_prop);
        } else {//Requested directory with /. Search for index files or list directory

            char* index_name=&connection_prop->strfile[connection_prop->strfile_len];//Pointer to where to write the filename
            int i;

            //Cyclyng through the indexes
            for (i=0; i<indexes_l; i++) {
                snprintf(index_name,INDEXMAXLEN,"%s",indexes[i]);//Add INDEX to the url
                if (file_exists(connection_prop->strfile)) { //If index exists, redirect to it
                    char head[URI_LEN+12];//12 is the size for the location header
                    snprintf(head,URI_LEN+12,"Location: %s%s\r\n",connection_prop->page,indexes[i]);
                    send_http_header_full(sock,303,0,head,true,-1,connection_prop);

                    index_found=true;
                    break;
                }
            }

            connection_prop->strfile[connection_prop->strfile_len]=0; //Removing the index part
            if (index_found==false) {//If no index was found in the dir
                write_dir(sock,real_basedir,connection_prop);
            }
        }
    } else {//Requested an existing file
        if (exec_script) { //Scripts enabled

            int q_;
            int f_len;
            for (q_=0; q_<cgi_paths.len; q_+=2) { //Check if it is a CGI script
                f_len=cgi_paths.data_l[q_];
                if (endsWith(connection_prop->page+connection_prop->page_len-f_len,cgi_paths.data[q_],f_len,f_len)) {
                    retval=exec_page(sock,cgi_paths.data[++q_],&post_param,real_basedir,connection_prop);
                    break;
                }
            }

            if (q_%2==0) { //Normal file
                retval= write_file(sock,connection_prop);
            }
        } else { //Scripts disabled
            retval= write_file(sock,connection_prop);
        }
    }

escape:
    free(post_param.data);
    free(connection_prop->strfile);

    //Closing local file previously opened
    if ((connection_prop->method_id==GET || connection_prop->method_id==POST) && connection_prop->strfile_fd>=0) {
        close(connection_prop->strfile_fd);
    }

    switch (retval) {
    case 0:
        return 0;
    case ERR_BRKPIPE:
        return send_err(sock,500,"Internal server error",connection_prop->ip_addr);
    case ERR_FILENOTFOUND:
        return send_err(sock,404,"Page not found",connection_prop->ip_addr);
    case ERR_NOMEM:
        return send_err(sock,503,"Service Unavailable",connection_prop->ip_addr);
    case ERR_NODATA:
    case ERR_NOTHTTP:
        return send_err(sock,400,"Bad request",connection_prop->ip_addr);
    case ERR_FORBIDDEN:
        return send_err(sock,403,"Forbidden",connection_prop->ip_addr);
    case ERR_NOTIMPLEMENTED:
        return send_err(sock,501,"Not implemented",connection_prop->ip_addr);
    case ERR_SERVICE_UNAVAILABLE:
        return send_err(sock,503,"Service Unavailable",connection_prop->ip_addr);
    case ERR_PRECONDITION_FAILED:
        return send_err(sock,412,"Precondition Failed",connection_prop->ip_addr);
    case ERR_CONFLICT:
        return send_err(sock,409,"Conflict",connection_prop->ip_addr);
    case ERR_INSUFFICIENT_STORAGE:
        return send_err(sock,507,"Insufficient Storage",connection_prop->ip_addr);
    case ERR_NOT_ALLOWED:
        return send_err(sock,405,"Method Not Allowed",connection_prop->ip_addr);
    case ERR_NOAUTH:
        return request_auth(sock,connection_prop->page);//Sends a request for authentication
    case OK_NOCONTENT:
        return send_http_header_full(sock,204,0,NULL,true,-1,connection_prop);
    case OK_CREATED:
        return send_http_header_full(sock,201,0,NULL,true,-1,connection_prop);
    }
    return 0; //Make gcc happy
}
/**
Executes a CGI script with a given interpreter and sends the resulting output
sock is the socket with the client
executor is the path to the binary which will execute the page
post_param contains the post data sent to the page (if present). This can't be null, but the string pointer inside the struct can be null.
real_basedir is the basedir (according to the virtualhost)
connection_prop is the struct containing all the data of the request

exec_page will fork and create pipes with the child.
The child will clean all the envvars and then set new ones as needed by CGI.
Then the child will call alarm to set the timeout to its execution, and then will exec the script.

*/
int exec_page(int sock,char * executor,string_t* post_param,char* real_basedir,connection_t* connection_prop) {
#ifdef SENDINGDBG
    syslog(LOG_INFO,"Executing file %s",connection_prop->strfile);
#endif

    int wpid;//Child's pid
    int wpipe[2];//Pipe's file descriptor
    int ipipe[2];//Pipe's file descriptor, used to pass POST on script's standard input

    //Pipe created and used only if there is POST data to send to the script
    if (post_param->data!=NULL) {
        pipe(ipipe);//Pipe to comunicate with the child
    }

    //Pipe to get the output of the child
    pipe(wpipe);//Pipe to comunicate with the child

    if ((wpid=fork())<0) { //Error, returns a no memory error
#ifdef SENDINGDBG
        syslog(LOG_CRIT,"Unable to fork to execute the file %s",connection_prop->strfile);
#endif
        if (post_param->data!=NULL) {
            close(ipipe[0]);
            close(ipipe[1]);
        }
        close(wpipe[1]);
        close(wpipe[0]);
        return ERR_NOMEM;
    } else if (wpid==0) { //Child, executes the script
        close (wpipe[0]); //Closes unused end of the pipe

        fclose (stdout); //Closing the stdout
        dup(wpipe[1]); //Redirects the stdout

#ifdef HIDE_CGI_ERRORS
        fclose (stderr); //Not printing any error
#endif
        //Redirecting standard input only if there is POST data
        if (post_param->data!=NULL) {//Send post data to script's stdin
            fclose(stdin);
            dup(ipipe[0]);
        }

        {
            //Clears all the env var, saving only SERVER_PORT
            char *port=getenv("SERVER_PORT");
            environ=NULL;
            setenv("SERVER_PORT",port,true);
        }
        setEnvVars(connection_prop->http_param); //Sets env var starting with HTTP
        {
            //Sets SERVER_ADDR var
#ifdef IPV6
            char loc_addr[INET6_ADDRSTRLEN];
            struct sockaddr_in6 addr;//Local and remote address
            socklen_t addr_l=sizeof(struct sockaddr_in6);

            getsockname(sock, (struct sockaddr *)&addr, &addr_l);
            inet_ntop(AF_INET6, &addr.sin6_addr,(char*)&loc_addr, INET6_ADDRSTRLEN);
#else
            char loc_addr[INET_ADDRSTRLEN];
            struct sockaddr_in addr;
            int addr_l=sizeof(struct sockaddr_in);

            getsockname(sock, (struct sockaddr *)&addr,(socklen_t *) &addr_l);
            inet_ntop(AF_INET, &addr.sin_addr,(char*)&loc_addr, INET_ADDRSTRLEN);
#endif

            setenv("SERVER_ADDR",(char*)&loc_addr,true);
        }

        //Set CGI needed vars
        setenv("SERVER_SIGNATURE",SIGNATURE,true);
        setenv("SERVER_SOFTWARE",SIGNATURE,true);
        setenv("GATEWAY_INTERFACE","CGI/1.1",true);
        setenv("REQUEST_METHOD",connection_prop->method,true); //POST GET
        setenv("SERVER_NAME", getenv("HTTP_HOST"),true); //TODO for older http version this header might not exist
        setenv("REDIRECT_STATUS","Ciao",true); // Mah.. i'll never understand php, this env var is needed
        setenv("SCRIPT_FILENAME",connection_prop->strfile,true); //This var is needed as well or php say no input file...
        setenv("DOCUMENT_ROOT",real_basedir,true);

        setenv("REMOTE_ADDR",connection_prop->ip_addr,true); //Client's address
        setenv("SCRIPT_NAME",connection_prop->page,true); //Name of the script without complete path

        //Request URI with or without a query
        if (connection_prop->get_params==NULL) {
            setenv("REQUEST_URI",connection_prop->page,true);
            setenv("QUERY_STRING","",true);//Query after ?
        } else {
            setenv("QUERY_STRING",connection_prop->get_params,true);//Query after ?

            //file and params were the same string.
            //Joining them again temporarily
            int delim=connection_prop->page_len;
            connection_prop->page[delim]='?';
            setenv("REQUEST_URI",connection_prop->page,true);
            connection_prop->page[delim]='\0';
        }

        {
            //If Content-Length field exists
            char *content_l=getenv("HTTP_CONTENT_LENGTH");
            if (content_l!=NULL) {
                setenv("CONTENT_LENGTH",content_l,true);
                setenv("CONTENT_TYPE",getenv("HTTP_CONTENT_TYPE"),true);
            }
        }

        {
            //chdir to the directory
            char* last_slash=rindex(connection_prop->strfile,'/');
            last_slash[0]=0;
            chdir(connection_prop->strfile);
        }

        alarm(SCRPT_TIMEOUT);//Sets the timeout for the script

        execl(executor,executor,(char *)0);
#ifdef SERVERDBG
        syslog(LOG_ERR,"Execution of %s failed",executor);
        perror("Execution of the page failed");
#endif
        exit(1);

    } else { //Father: reads from pipe and sends
        //Closing pipes, so if they're empty read is non blocking
        close (wpipe[1]);

        //Large buffer, must contain the output of the script
        char* header_buf=malloc(MAXSCRIPTOUT+HEADBUF);

        if (header_buf==NULL) { //Was unable to allocate the buffer
            int state;
#ifdef SERVERDBG
            syslog(LOG_CRIT,"Not enough memory to allocate buffers for CGI");
#endif
            close (wpipe[0]);
            if (post_param->data!=NULL) {//Pipe created and used only if there is data to send to the script
                close (ipipe[0]); //Closes unused end of the pipe
                close (ipipe[1]); //Closes the pipe
            }
            kill(wpid,SIGKILL); //Kills cgi process
            waitpid (wpid,&state,0); //Removes zombie process
            return ERR_NOMEM;//Returns if buffer was not allocated
        }

        if (post_param->data!=NULL) {//Pipe created and used only if there is data to send to the script
            //Send post data to script's stdin
            write(ipipe[1],post_param->data,post_param->len);
            close (ipipe[0]); //Closes unused end of the pipe
            close (ipipe[1]); //Closes the pipe
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

        if (e_reads>0) {//There is output from script
            char* status; //Standard status
            {
                //Reading if there is another status
                char*s=strstr(header_buf,"Status: ");
                if (s!=NULL) {
                    status=s+8; //Replacing status
                } else {
                    status="200"; //Standard status
                }
            }

            /* There could be other data, which didn't fit in the buffer,
            so we set reads to -1 (this will make connection non-keep-alive)
            and we continue reading and writing to the socket */
            if (e_reads==MAXSCRIPTOUT+HEADBUF) {
                reads=-1;
                connection_prop->keep_alive=false;
            }

            send_http_header_scode(sock,status,reads,header_buf);

            if (reads!=0) {//Sends the page if there is something to send
                write (sock,scrpt_buf,reads);
            }

            if (reads==-1) {//Reading until the pipe is empty, if it wasn't fully read before
                e_reads=MAXSCRIPTOUT+HEADBUF;
                while (e_reads==MAXSCRIPTOUT+HEADBUF) {
                    e_reads=read(wpipe[0],header_buf,MAXSCRIPTOUT+HEADBUF);
                    write (sock,header_buf,e_reads);
                }
            }

            //Closing pipe
            close (wpipe[0]);


        } else {//No output from script, maybe terminated...
            send_err(sock,500,"Internal server error",connection_prop->ip_addr);
        }

        free(header_buf);

    }
    return 0;
}

/**
This function writes on the specified socket an html page containing the list of files within the
specified directory.
*/
int write_dir(int sock,char* real_basedir,connection_t* connection_prop) {
    /*
    Determines if has to show the link to parent dir or not.
    If page is the basedir, it won't show the link to ..
    */
    int pagelen;
    bool parent=true;

    {
        size_t basedir_len=strlen(real_basedir);
        if (connection_prop->strfile_len-1==basedir_len || connection_prop->strfile_len==basedir_len)
            parent=false;
    }

    char* html=malloc(MAXSCRIPTOUT);//Memory for the html page
    if (html==NULL) { //No memory
#ifdef SERVERDBG
        syslog(LOG_CRIT,"Not enough memory to allocate buffers to list directory");
#endif
        return ERR_NOMEM;
    }

    if ((pagelen=list_dir (connection_prop,html,MAXSCRIPTOUT,parent))<0) { //Creates the page
        free(html);//Frees the memory used for the page
        return ERR_FILENOTFOUND;
    } else { //If there are no errors sends the page
        send_http_header_full(sock,200,pagelen,NULL,true,-1,connection_prop);
        write(sock,html,pagelen);
    }

    free(html);//Frees the memory used for the page

    return 0;
}

/**
Writes a file to the socket, compressing it with gzip.
Then, since it is not possible to know the size of the compressed file,
it is not possible to send the size in advance, so it will close the connection
(and will seand the header "Connection: close" before) after sending.

sock is the socket
strfile is the file to compress and send
size is the size of the uncompressed file
*/
#ifdef __COMPRESSION
int write_compressed_file(int sock,unsigned int size,time_t timestamp,connection_t* connection_prop ) {
    connection_prop->keep_alive=false;
    send_http_header_full(sock,200,size,"Connection: close\r\nContent-Encoding: gzip\r\n",false,timestamp,connection_prop);
    int pid=fork();

    if (pid==0) { //child, executing gzip
        fclose (stdout); //Closing the stdout
        dup(sock); //Redirects the stdout
#ifdef GZIPNICE
        nice(GZIPNICE); //Reducing priority
#endif
        execlp("gzip","gzip","-c",connection_prop->strfile,(char *)0);

    } else if (pid>0) { //Father, does nothing
        int status;
        waitpid(pid,&status,0);
    } else { //Error
        return ERR_NOMEM; //Well not enough memory in process table...
    }
    close(sock);
    return 0;
}
#endif

/**
This function reads a file and writes it to the socket.
Also sends the http header with the content length header
same as the file's size.
There is no limit to file size, and it uses O_LARGEFILE flag
to send file larger than 4gb. Since this flag isn't available on
all systems, it will be 0 when compiled on system who doesn't have
this flag. On those systems it is unpredictable if it will be able to
send large files or not.

If the file is larger, it will be sent using write_compressed_file,
see that function for details.
*/
int write_file(int sock,connection_t* connection_prop) {
    char a[RBUFFER]; //Buffer for Range, Content-Range headers, and reading if-none-match from header

    //If the file has the same date, there is no need of sending it again
    if (get_param_value(connection_prop->http_param,"If-None-Match",a,RBUFFER,13)) {//Find if it is a range request, 13 is strlen of "if-none-match"
        time_t etag=(time_t)strtol(a+1,NULL,0);
        if (connection_prop->strfile_stat.st_mtime==etag) {//Browser has the item in its cache, sending 304
            send_http_header_full(sock,304,0,NULL,true,etag,connection_prop);
            return 0;
        }
    }
#ifdef __COMPRESSION
    if (connection_prop->strfile_stat.st_size>SIZE_COMPRESS_MIN && connection_prop->strfile_stat.st_size<SIZE_COMPRESS_MAX) { //Using compressed file method instead of sending it raw
        char *accept;
        char *end;

        if ((accept=strstr(connection_prop->http_param,"Accept-Encoding:"))!=NULL && (end=strstr(accept,"\r\n"))!=NULL) {
            

            //Avoid to parse the entire header.
            end[0]='\0';
            char* gzip=strstr(accept,"gzip");
            end[0]='\r';

            if (gzip!=NULL) {
                return write_compressed_file(sock,stat_f.st_size,stat_f.st_mtime,connection_prop);
            }
        }
    }
#endif

    char* buf=malloc(FILEBUF);//Buffer to read from file
    if (buf==NULL) {
#ifdef SERVERDBG
        syslog(LOG_CRIT,"Not enough memory to allocate buffers");
#endif
        return ERR_NOMEM;//If no memory is available
    }

    off_t count=connection_prop->strfile_stat.st_size;//Bytes to send to the client

#ifdef __RANGE
    if (get_param_value(connection_prop->http_param,"Range",a,RBUFFER,5)) {//Find if it is a range request 5 is strlen of "range"
        int from,to;

        {
            //Locating from and to
            //Range: bytes=12323-123401
            char *eq, *sep;
            if ((eq=strstr(a,"="))==NULL||(sep=strstr(eq,"-"))==NULL) {//Invalid data in Range header.
                free(buf);
                return ERR_NOTHTTP;
            }
            sep[0]=0;
            from=strtol(eq+1,NULL,0);
            to=strtol(sep+1,NULL,0);
        }

        if (to==0) { //If no to is specified, it is to the end of the file
            to=connection_prop->strfile_stat.st_size-1;
        }
        snprintf(a,RBUFFER,"Accept-Ranges: bytes\r\nContent-Range: bytes=%d-%d/%d\r\n",from,to,(int)connection_prop->strfile_stat.st_size);
        lseek(connection_prop->strfile_fd,from,SEEK_SET);
        count=to-from+1;

        send_http_header_full(sock,206,count,a,true,connection_prop->strfile_stat.st_mtime,connection_prop);

    } else //Normal request
#endif
    {
        send_http_header_full(sock,200,connection_prop->strfile_stat.st_size,NULL,true,connection_prop->strfile_stat.st_mtime,connection_prop);
    }

    int reads,wrote;

    //Sends file
    while (count>0 && (reads=read(connection_prop->strfile_fd, buf, FILEBUF<count? FILEBUF:count ))!=0) {
        count-=reads;
        wrote=write(sock,buf,reads);
        if (wrote!=reads) { //Error writing to the socket
#ifdef SOCKETDBG
            syslog(LOG_ERR,"Unable to send %s: error writing to the socket",connection_prop->strfile);
#endif
            break;
        }
    }

    free(buf);
    return 0;
}


/**
Sends a request for authentication
*/
int request_auth(int sock,char * descr) {

    //Buffer for both header and page
    char * head=malloc(MAXSCRIPTOUT+HEADBUF);
    if (head==NULL) {
#ifdef SERVERDBG
        syslog(LOG_CRIT,"Not enough memory to allocate buffers");
#endif
        return ERR_NOMEM;
    }

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

    if (head==NULL) {
#ifdef SERVERDBG
        syslog(LOG_CRIT,"Not enough memory to allocate buffers");
#endif
        return ERR_NOMEM;
    }

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
int send_http_header_scode(int sock,char* code, int size,char* headers) {

    char *head=malloc(HEADBUF);
    if (head==NULL) {
#ifdef SERVERDBG
        syslog(LOG_CRIT,"Not enough memory to allocate buffers");
#endif
        return ERR_NOMEM;
    }

    int len_head;
    if (size>=0) {
        len_head=snprintf(head,HEADBUF,"HTTP/1.1 %c%c%c OK Server: Weborf (GNU/Linux)\r\nContent-Length: %u\r\n%s\r\n",code[0],code[1],code[2], size,headers);
    } else {
        len_head=snprintf(head,HEADBUF,"HTTP/1.1 %c%c%c OK Server: Weborf (GNU/Linux)\r\nConnection: close\r\n%s\r\n",code[0],code[1],code[2],headers);
    }

    int wrote=write (sock,head,len_head);
    free(head);
    if (wrote!=len_head) return ERR_BRKPIPE;
    return 0;
}

/**
This function checks if the authentication can be granted or not calling the external program.
Returns 0 if authorization is granted.
*/
int check_auth(int sock, connection_t* connection_prop) {
    char username[PWDLIMIT*2];
    char* password=username; //will be changed if there is a password

    char* auth=strstr(connection_prop->http_param,"Authorization: Basic ");//Locates the auth information
    if (auth==NULL) { //No auth informations
        username[0]=0;
        //password[0]=0;
    } else { //Retrieves provided username and password
        char*auth_end=strstr(auth,"\r\n");//Finds the end of the header
        if (auth_end==NULL) return -1;
        
        char a[PWDLIMIT*2];
        auth+=21;//Moves the begin of the string to exclude Authorization: Basic
        if ((auth_end-auth+1)<(PWDLIMIT*2))
            memcpy(&a,auth,auth_end-auth); //Copies the base64 encoded string to a temp buffer
        else { //Auth string is too long for the buffer
#ifdef SERVERDBG
            syslog(LOG_ERR,"Unable to accept authentication, buffer is too small");
#endif
            return ERR_NOMEM;
        }

        a[auth_end-auth]=0;
        decode64(username,a);//Decodes the base64 string

        if ((password=strstr(username,":"))!=NULL) {;//Locates the separator :
            password[0]=0;//Nulls the separator to split username and password
            password++;//make password point to the beginning of the password
        }
    }

    short int result=-1;
    {
        int s,len;
        struct sockaddr_un remote;
        s=socket(AF_UNIX,SOCK_STREAM,0);

        remote.sun_family = AF_UNIX;
        strcpy(remote.sun_path, authsock);
        len = strlen(remote.sun_path) + sizeof(remote.sun_family);
        if (connect(s, (struct sockaddr *)&remote, len) == -1) {//Unable to connect
            return -1;
        }
        char* auth_str=malloc(HEADBUF+PWDLIMIT*2);
        if (auth_str==NULL) {
#ifdef SERVERDBG
            syslog(LOG_CRIT,"Not enough memory to allocate buffers");
#endif
            return -1;
        }

        int auth_str_l=snprintf(auth_str,HEADBUF+PWDLIMIT*2,"%s\r\n%s\r\n%s\r\n%s\r\n%s\r\n%s\r\n",connection_prop->page,connection_prop->ip_addr,connection_prop->method,username,password,connection_prop->http_param);
        if (write(s,auth_str,auth_str_l)==auth_str_l && read(s,auth_str,1)==0) {//All data written and no output, ok
            result=0;
        }

        close(s);
        free(auth_str);
    }

    return result;
}

/**
This function reads post data and returns the pointer to the buffer containing the data (if there is any)
or NULL if there was no data.
If it doesn't return a null value, the returned pointer must be freed.
*/
string_t read_post_data(int sock,connection_t* connection_prop,buffered_read_t* read_b) {
    string_t res;
    res.len=0;
    res.data=NULL;

    //Buffer for field's value
    char a[NBUFFER];
    //Gets the value
    bool r=get_param_value(connection_prop->http_param,"Content-Length", a,NBUFFER,14);//14 is content-lenght's len

    //If there is a value and method is POST
    if (r!=false) { //&& connection_prop->method_id==POST) {
        long int l=strtol( a , NULL, 0 );
        if (l<=POST_MAX_SIZE && (res.data=malloc(l))!=NULL) {//Post size is ok and buffer is allocated
            res.len=buffer_read(sock,res.data,l,read_b);
        }
    }
    return res;
}

/**
This function returns a pointer to a string.
It will use virtualhost settings to return this string and if no virtualhost is set for that host, it will return
the default basedir.
Those string must
*/
char* get_basedir(char* http_param) {
    if (virtual_host==false) return basedir;

    char* result;
    char* h=strstr(http_param,"\r\nHost: ");

    if (h==NULL) return basedir;

    h+=8;//Removing "Host:" string
    char* end=strstr(h,"\r");
    if (end==NULL) return basedir;
    end[0]=0;
    result=getenv(h);
    end[0]='\r';

    if (result==NULL) return basedir; //Reqeusted host doesn't exist

    return result;
}


/**
This function sends a code header to the specified socket
size is the Content-Length field.
headers can be NULL or some extra headers to add. Headers must be
separated by \r\n and must have an \r\n at the end.

Content says if the size is for content-lenght or for entity-length

Timestamp is the timestamp for the content. Set to -1 to use the current timestamp
*/
int send_http_header_full(int sock,int code, unsigned int size,char* headers,bool content,time_t timestamp,connection_t* connection_prop) {
    int len_head,wrote;
    char *head=malloc(HEADBUF);
    char* h_ptr=head;
    int left_head=HEADBUF;

    if (head==NULL) {
#ifdef SERVERDBG
        syslog(LOG_CRIT,"Not enough memory to allocate buffers");
#endif
        return ERR_NOMEM;
    }
    if (headers==NULL) headers="";
    if (connection_prop->protocol_version!=HTTP_1_1 && connection_prop->keep_alive==true) {
        //Putting explicit keep-alive header, since client is not 1.1 but is using keep alive
        len_head=snprintf(head,HEADBUF,"HTTP/1.1 %d\r\nServer: Weborf (GNU/Linux)\r\nConnection: Keep-Alive\r\n",code);
    } else {
        len_head=snprintf(head,HEADBUF,"HTTP/1.1 %d\r\nServer: Weborf (GNU/Linux)\r\n",code);
    }
    //This stuff moves the pointer to the buffer forward, and reduces the left space in the buffer itself
    //Next snprintf will append their strings to the buffer, without overwriting.
    head+=len_head;
    left_head-=len_head;

    //Creating ETag and date from timestamp
    if (timestamp!=-1) {
        //Sends ETag, if a timestamp is set
        len_head = snprintf(head,left_head,"ETag: \"%d\"\r\n",(int)timestamp);
        head+=len_head;
        left_head-=len_head;
    }
#ifdef SEND_LAST_MODIFIED_HEADER
    else {//timestamp with now, to be eventually used by Last-Modified
        timestamp=time(NULL);
    }

    {
        //Sends Date
        struct tm  ts;
        localtime_r((time_t)&timestamp,&ts);
        len_head = strftime(head,left_head, "Last-Modified: %a, %d %b %Y %H:%M:%S GMT\r\n", &ts);
        head+=len_head;
        left_head-=len_head;
    }
#endif

    if (size>0 || (connection_prop->keep_alive==true && size==0)) {
        //Content length (or entity lenght) and extra headers
        if (content) {
            len_head=snprintf(head,left_head,"Content-Length: %u\r\n",size);
        } else {
            len_head=snprintf(head,left_head,"entity-length: %u\r\n",size);
        }

        head+=len_head;
        left_head-=len_head;
    }

    len_head=snprintf(head,left_head,"%s\r\n",headers);
    head+=len_head;
    left_head-=len_head;


    wrote=write (sock,h_ptr,HEADBUF-left_head);
    free(h_ptr);
    if (wrote!=HEADBUF-left_head) return ERR_BRKPIPE;
    return 0;
}
