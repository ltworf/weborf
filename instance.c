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
#include "options.h"

#include <time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <syslog.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/un.h>
#include <errno.h>


#include "utils.h"
#include "myio.h"
#include "cgi.h"
#include "queue.h"
#include "mystring.h"
#include "mime.h"
#include "instance.h"
#include "cachedir.h"
#include "types.h"
#include "auth.h"

extern syn_queue_t queue;                   //Queue for open sockets

extern t_thread_info thread_info;

extern weborf_configuration_t weborf_conf;

extern char* indexes[MAXINDEXCOUNT];        //Array containing index files
extern int indexes_l;                       //Length of array
extern pthread_key_t thread_key;            //key for pthread_setspecific



int request_auth(connection_t *connection_prop);
void piperr();
int write_dir(char *real_basedir, connection_t * connection_prop);
static int tar_send_dir(connection_t* connection_prop);
static int send_page(buffered_read_t* read_b, connection_t* connection_prop);
static int send_error_header(int retval, connection_t *connection_prop);
static int get_or_post(connection_t *connection_prop, string_t post_param);

/**
Checks if the required resource has the same date as the one cached in the client.
If they are the same, returns 0,
returns 1 otherwise

If the ETag matches this function will send to the client a 304 response too, so
if this function returns 0, the HTTP request has been already served.

The char* buffer must be at least RBUFFER bytes (see definitions in options.h)
*/
static inline int check_etag(connection_t* connection_prop,char *a) {
    //Find if it is a range request, 13 is strlen of "if-none-match"
    char *if_none_match="If-None-Match";
    if (get_param_value(connection_prop->http_param,if_none_match,a,RBUFFER,strlen(if_none_match))) {
        time_t etag=(time_t)strtol(a+1,NULL,0);
        if (connection_prop->strfile_stat.st_mtime==etag) {
            //Browser has the item in its cache, sending 304
            send_http_header(304,0,NULL,true,etag,connection_prop);
            return 0;
        }
    }
    return 1;
}

/**
This function does some changes on the URL.
The url will never be longer than the original one.
*/
static inline void modURL(char* url) {
    replaceEscape(url);

    //Prevents the use of .. to access the whole filesystem
    strReplace(url,"../",'\0');

    //TODO AbsoluteURI: Check if the url uses absolute url, and in that case remove the 1st part
}

/**
Sets keep_alive and protocol_version fields of connection_t
*/
static inline void set_connection_props(connection_t *connection_prop) {
    char a[12];//Gets the value
    //Obtains the connection header, writing it into the a buffer, and sets connection=true if the header is present
    bool connection=get_param_value(connection_prop->http_param,"Connection", a,sizeof(a),strlen("Connection"));

    //Setting the connection type, using protocol version
    if (connection_prop->http_param[7]=='1' && connection_prop->http_param[5]=='1') {//Keep alive by default (protocol 1.1)
        connection_prop->protocol_version=HTTP_1_1;
        connection_prop->keep_alive=(connection && strncmp(a,"close",5)==0)?false:true;
    } else {//Not http1.1
        //Constants are set to make this line work
        connection_prop->protocol_version=connection_prop->http_param[7];
        connection_prop->keep_alive=(connection && strncmp(a,"Keep",4)==0)?true:false;
    }

    modURL(connection_prop->page);//Operations on the url string
    split_get_params(connection_prop);//Splits URI into page and parameters
    connection_prop->basedir=get_basedir(connection_prop->http_param);
}

static inline void handle_requests(char* buf,buffered_read_t * read_b,int * bufFull,connection_t* connection_prop,long int id) {
    int from;
    int sock=connection_prop->sock;
    char *lasts;//Used by strtok_r


    short int r;//Readed char
    char *end;//Pointer to header's end

    while (true) { //Infinite cycle to handle all pipelined requests
        if ((*bufFull)!=0) {
            memset(buf,0,(*bufFull));//Sets to 0 the buffer, only the part used for the previous request in the same connection
            (*bufFull)=0;//bufFull-(end-buf+4);
        }
        from=0;

        while ((end=strstr(buf+from,"\r\n\r"))==NULL) { //Determines if there is a \r\n\r which is an ending sequence
            //ssize_t rsize=buffer_strstr(sock,read_b,"\r\n\r\n");
            //r=buffer_read(sock, buf+(*bufFull),rsize+strlen("\r\n\r\n"),read_b);//Reads 2 char and adds to the buffer
            r=buffer_read(sock, buf+(*bufFull),2,read_b);//Reads 2 char and adds to the buffer

            if (r<=0) { //Connection closed or error
                return;
            }

            if (!(buf[*bufFull]==10 || buf[*bufFull]==13)) {//Optimization to make strstr parse only the ending part of the string
                from=(*bufFull);
            }

            //TODO remove this crap!!
            //if ((*bufFull)!=0) { //Removes Cr Lf from beginning
            (*bufFull)+=r;//Sets the end of the user buffer (may contain more than one header)
            //} else if (buf[*bufFull]!='\n' && buf[*bufFull]!='\r') {
            //    (*bufFull)+=r;
            //}

            //Buffer full and still no valid http header
            if ((*bufFull)>=INBUFFER) goto bad_request;

        }

        if (strstr(buf+from,"\r\n\r\n")==NULL) {//If we didn't read yet the lst \n of the ending sequence, we read it now, so it won't disturb the next request
            *bufFull+=buffer_read(sock, buf+*bufFull,1,read_b);//Reads 1 char and adds to the buffer
        }

        end[2]='\0';//Terminates the header, leaving a final \r\n in it

        //Finds out request's kind
        if (strncmp(buf,"GET",strlen("GET"))==0) connection_prop->method_id=GET;
        else if (strncmp(buf,"POST",strlen("POST"))==0) connection_prop->method_id=POST;
        else if (strncmp(buf,"PUT",strlen("PUT"))==0) connection_prop->method_id=PUT;
        else if (strncmp(buf,"DELETE",strlen("DELETE"))==0) connection_prop->method_id=DELETE;
        else if (strncmp(buf,"OPTIONS",strlen("OPTIONS"))==0) connection_prop->method_id=OPTIONS;
#ifdef WEBDAV
        else if (strncmp(buf,"PROPFIND",strlen("PROPFIND"))==0) connection_prop->method_id=PROPFIND;
        else if (strncmp(buf,"MKCOL",strlen("MKCOL"))==0) connection_prop->method_id=MKCOL;
        else if (strncmp(buf,"COPY",strlen("COPY"))==0) connection_prop->method_id=COPY;
        else if (strncmp(buf,"MOVE",strlen("MOVE"))==0) connection_prop->method_id=MOVE;
#endif
        else goto bad_request;

        connection_prop->method=strtok_r(buf," ",&lasts);//Must be done to eliminate the request
        connection_prop->page=strtok_r(NULL," ",&lasts);
        if (connection_prop->page==NULL || connection_prop->method == NULL) goto bad_request;

        connection_prop->http_param=lasts;


#ifdef THREADDBG
        syslog(LOG_INFO,"Requested page: %s to Thread %ld",connection_prop->page,id);
#endif
        //Stores the parameters of the request
        set_connection_props(connection_prop);

        if (send_page(read_b, connection_prop)<0) {
#ifdef REQUESTDBG
            syslog(LOG_INFO,"%s - FAILED - %s %s",connection_prop->ip_addr,connection_prop->method,connection_prop->page);
#endif

            close(sock);
            return;//Unable to send an error
        }

#ifdef REQUESTDBG
        syslog(LOG_INFO,"%s - %d - %s %s",connection_prop->ip_addr,connection_prop->status_code,connection_prop->method,connection_prop->page);
#endif


        //Non pipelined
        if (connection_prop->keep_alive==false) return;

    } /* while */

bad_request:
    send_err(connection_prop,400,"Bad request");
    close(sock);
    return;
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
    thread_prop_t thread_prop;  //Server's props
    pthread_setspecific(thread_key, (void *)&thread_prop); //Set thread_prop as thread variable

    //General init of the thread
    thread_prop.id=(long int)nulla;//Set thread's id
#ifdef THREADDBG
    syslog(LOG_DEBUG,"Starting thread %ld",thread_prop.id);
#endif

    //Vars
    int bufFull=0;                                  //Amount of buf used
    connection_t connection_prop;                   //Struct to contain properties of the connection
    buffered_read_t read_b;                         //Buffer for buffered reader
    int sock=0;                                     //Socket with the client
    char * buf=calloc(INBUFFER+1,sizeof(char));     //Buffer to contain the HTTP request
    connection_prop.strfile=malloc(URI_LEN);        //buffer for filename

    signal(SIGPIPE, SIG_IGN);//Ignores SIGPIPE

#ifdef IPV6
    struct sockaddr_in6 addr;//Local and remote address
    socklen_t addr_l=sizeof(struct sockaddr_in6);
#else
    struct sockaddr_in addr;
    int addr_l=sizeof(struct sockaddr_in);
#endif

    if (mime_init(&thread_prop.mime_token)!=0 || buffer_init(&read_b,BUFFERED_READER_SIZE)!=0 || buf==NULL || connection_prop.strfile==NULL) { //Unable to allocate the buffer
#ifdef SERVERDBG
        syslog(LOG_CRIT,"Not enough memory to allocate buffers for new thread");
#endif
        goto release_resources;
    }

    //Start accepting sockets
    change_free_thread(thread_prop.id,1,0);

    while (true) {
        q_get(&queue, &sock);//Gets a socket from the queue
        change_free_thread(thread_prop.id,-1,0);//Sets this thread as busy

        if (sock<0) { //Was not a socket but a termination order
            goto release_resources;
        }

        connection_prop.sock=sock;//Assigned socket into the struct

        //Converting address to string
#ifdef IPV6
        getpeername(sock, (struct sockaddr *)&addr, &addr_l);
        inet_ntop(AF_INET6, &addr.sin6_addr, connection_prop.ip_addr, INET6_ADDRSTRLEN);
#else
        getpeername(sock, (struct sockaddr *)&addr,(socklen_t *) &addr_l);
        inet_ntop(AF_INET, &addr.sin_addr, connection_prop.ip_addr, INET_ADDRSTRLEN);
#endif

#ifdef THREADDBG
        syslog(LOG_DEBUG,"Thread %ld: Reading from socket",thread_prop.id);
#endif
        handle_requests(buf,&read_b,&bufFull,&connection_prop,thread_prop.id);

#ifdef THREADDBG
        syslog(LOG_DEBUG,"Thread %ld: Closing socket with client",thread_prop.id);
#endif

        close(sock);//Closing the socket
        buffer_reset (&read_b);

        change_free_thread(thread_prop.id,1,0);//Sets this thread as free
    }



release_resources:
#ifdef THREADDBG
    syslog(LOG_DEBUG,"Terminating thread %ld",thread_prop.id);
#endif
    free(buf);
    free(connection_prop.strfile);
    buffer_free(&read_b);
    mime_release(thread_prop.mime_token);
    change_free_thread(thread_prop.id,0,-1);//Reduces count of threads
    pthread_exit(0);
    return NULL;//Never reached
}

/**
This function handles a PUT request.

It requires the socket, connection_t struct and buffered_read_t to read from
the socket, since the reading is internally buffered.

PUT size has no hardcoded limits.
Auth provider has to check for the file's size and refuse it if it is the case.
This function will not work if there is no auth provider.
*/
int read_file(connection_t* connection_prop,buffered_read_t* read_b) {
    int sock=connection_prop->sock;
    if (weborf_conf.authsock==NULL) {
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
    int retval;
    long long int content_l;  //Length of the put data

    //Gets the value of content-length header
    bool r=get_param_value(connection_prop->http_param,"Content-Length", a,NBUFFER,strlen("Content-Length"));//14 is content-lenght's len


    if (r!=false) {//If there is no content-lenght returns error
        content_l=strtoll( a , NULL, 0 );
    } else {//No data
        return ERR_NODATA;
    }

    //Checks if file already exists or not (needed for response code)
    if (file_exists(connection_prop->strfile)) {//Resource already existed (No content)
        retval=OK_NOCONTENT;
    } else {//Resource was created (Created)
        retval=OK_CREATED;
    }

    int fd=open(connection_prop->strfile,O_WRONLY|O_CREAT,S_IRUSR|S_IWUSR);
    if (fd<0) {
        return ERR_FILENOTFOUND;
    }
    
    ftruncate(fd,content_l);

    char* buf=malloc(FILEBUF);//Buffer to read from file
    if (buf==NULL) {
#ifdef SERVERDBG
        syslog(LOG_CRIT,"Not enough memory to allocate buffers");
#endif
        close(fd);
        return ERR_NOMEM;
    }

    long long int read_,write_;
    long long int tot_read=0;
    long long int to_read;

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
int delete_file(connection_t* connection_prop) {
    int retval;

    if (weborf_conf.authsock==NULL) {
        return ERR_FORBIDDEN;
    }

    //connection_prop->strfile
    struct stat stat_d;
    if (stat(connection_prop->strfile,&stat_d)<0) {
        return ERR_FILENOTFOUND;
    }

    if (S_ISDIR(stat_d.st_mode)) {
        retval=dir_remove(connection_prop->strfile);
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
static inline int options (connection_t* connection_prop) {

#ifdef WEBDAV
#define ALLOWED "Allow: GET,POST,PUT,DELETE,OPTIONS,PROPFIND,MKCOL,COPY,MOVE\r\nDAV: 1,2\r\nDAV: <http://apache.org/dav/propset/fs/1>\r\nMS-Author-Via: DAV\r\n"
#else
#define ALLOWED "Allow: GET,POST,PUT,DELETE,OPTIONS\r\n"
#endif

    send_http_header(200,0,ALLOWED,true,-1,connection_prop);
    return 0;
}

/**
This function determines the requested page and sends it
http_param is a string containing parameters of the HTTP request
*/
static int send_page(buffered_read_t* read_b, connection_t* connection_prop) {
    int retval=0;//Return value after sending the page
    string_t post_param; //Contains POST data

#ifdef SENDINGDBG
    syslog (LOG_DEBUG,"URL changed into %s",connection_prop->page);
#endif

    if (auth_check_request(connection_prop)!=0) { //If auth is required
        retval = ERR_NOAUTH;
        post_param.data=NULL;
        goto escape;
    }

    connection_prop->strfile_len = snprintf(connection_prop->strfile,URI_LEN,"%s%s",connection_prop->basedir,connection_prop->page);//Prepares the string

    if (connection_prop->method_id>=PUT) {//Methods from PUT to other uncommon ones :-D
        post_param.data=NULL;
        post_param.len=0;

        switch (connection_prop->method_id) {
        case PUT:
            retval=read_file(connection_prop,read_b);
            break;
        case DELETE:
            retval=delete_file(connection_prop);
            break;
        case OPTIONS:
            retval=options(connection_prop);
            break;
#ifdef WEBDAV
        case PROPFIND:
            //Propfind has data, not strictly post but read_post_data will work
            post_param=read_post_data(connection_prop,read_b);
            retval=propfind(connection_prop,&post_param);
            break;
        case MKCOL:
            retval=mkcol(connection_prop);
            break;
        case COPY:
        case MOVE:
            retval=copy_move(connection_prop);
            break;
#endif
        }

        goto escape;
    }

    if (connection_prop->method_id==POST)
        post_param=read_post_data(connection_prop,read_b);
    else
        post_param.data=NULL;

    if ((connection_prop->strfile_fd=open(connection_prop->strfile,O_RDONLY | O_LARGEFILE))<0) {
        //File doesn't exist. Must return errorcode
        retval=ERR_FILENOTFOUND;
        goto escape;
    }

    fstat(connection_prop->strfile_fd, &connection_prop->strfile_stat);

    retval = get_or_post(connection_prop,post_param);

escape:
    free(post_param.data);

    //Closing local file previously opened
    if ((connection_prop->method_id==GET || connection_prop->method_id==POST) && connection_prop->strfile_fd>=0) {
        close(connection_prop->strfile_fd);
    }

    return send_error_header(retval,connection_prop);
}

/**
 * With get or post this function is called, it decides if execute CGI or send
 * the simple file, and if the request points to a directory it will redirect to
 * the appropriate index file or show the list of the files.
 * */
static int get_or_post(connection_t *connection_prop, string_t post_param) {

    if (S_ISDIR(connection_prop->strfile_stat.st_mode)) {//Requested a directory

        if (weborf_conf.tar_directory) {
            return tar_send_dir(connection_prop);
        }

        //Requested a directory without ending /
        if (!endsWith(connection_prop->strfile,"/",connection_prop->strfile_len,1)) {//Putting the ending / and redirect
            char head[URI_LEN+12];//12 is the size for the location header
            snprintf(head,URI_LEN+12,"Location: %s/\r\n",connection_prop->page);
            send_http_header(301,0,head,true,-1,connection_prop);
            return 0;
        } else {//Requested directory with "/" Search for index files or list directory

            char* index_name=&connection_prop->strfile[connection_prop->strfile_len];//Pointer to where to write the filename
            int i;

            //Cyclyng through the indexes
            for (i=0; i<weborf_conf.indexes_l; i++) {
                snprintf(index_name,INDEXMAXLEN,"%s",weborf_conf.indexes[i]);//Add INDEX to the url
                if (file_exists(connection_prop->strfile)) { //If index exists, redirect to it
                    char head[URI_LEN+12];//12 is the size for the location header
                    snprintf(head,URI_LEN+12,"Location: %s%s\r\n",connection_prop->page,weborf_conf.indexes[i]);
                    send_http_header(303,0,head,true,-1,connection_prop);
                    return 0;
                }
            }

            connection_prop->strfile[connection_prop->strfile_len]=0; //Removing the index part
            return write_dir(connection_prop->basedir,connection_prop);
        }


    } else {//Requested an existing file
        if (weborf_conf.exec_script) { //Scripts enabled

            int q_;
            int f_len;
            for (q_=0; q_<weborf_conf.cgi_paths.len; q_+=2) { //Check if it is a CGI script
                f_len=weborf_conf.cgi_paths.data_l[q_];
                if (endsWith(connection_prop->page+connection_prop->page_len-f_len,weborf_conf.cgi_paths.data[q_],f_len,f_len)) {
                    return exec_page(weborf_conf.cgi_paths.data[++q_],&post_param,connection_prop->basedir,connection_prop);
                }
            }
        }

        //send normal file, control reaches this point if scripts are disabled or if the filename doesn't trigger CGI
        return write_file(connection_prop);


    }
    //return 0; //make gcc happy
}

/**
 * If retval is not 0, this function will send an appropriate error
 * or request authentication or any other header that does not imply
 * sending some content as well.
 * */
static int send_error_header(int retval, connection_t *connection_prop) {
    switch (retval) {
    case 0:
        return 0;
    case ERR_BRKPIPE:
        return send_err(connection_prop,500,"Internal server error");
    case ERR_FILENOTFOUND:
        return send_err(connection_prop,404,"Page not found");
    case ERR_NOMEM:
        return send_err(connection_prop,503,"Service Unavailable");
    case ERR_NODATA:
    case ERR_NOTHTTP:
        return send_err(connection_prop,400,"Bad request");
    case ERR_FORBIDDEN:
        return send_err(connection_prop,403,"Forbidden");
    case ERR_NOTIMPLEMENTED:
        return send_err(connection_prop,501,"Not implemented");
    case ERR_SERVICE_UNAVAILABLE:
        return send_err(connection_prop,503,"Service Unavailable");
    case ERR_PRECONDITION_FAILED:
        return send_err(connection_prop,412,"Precondition Failed");
    case ERR_CONFLICT:
        return send_err(connection_prop,409,"Conflict");
    case ERR_INSUFFICIENT_STORAGE:
        return send_err(connection_prop,507,"Insufficient Storage");
    case ERR_NOT_ALLOWED:
        return send_err(connection_prop,405,"Method Not Allowed");
    case ERR_NOAUTH:
        return request_auth(connection_prop);//Sends a request for authentication
    case OK_NOCONTENT:
        return send_http_header(204,0,NULL,true,-1,connection_prop);
    case OK_CREATED:
        return send_http_header(201,0,NULL,true,-1,connection_prop);
    }
    return 0; //Make gcc happy

}

/**
This function writes on the specified socket an html page containing the list of files within the
specified directory.
*/
int write_dir(char* real_basedir,connection_t* connection_prop) {
    int sock=connection_prop->sock;

    /*
    WARNING
    This code checks the ETag and returns if the client has a copy in cache
    since the impact of using ETag for generated directory list is not known
    yet, if ETag goes away, also the following block will have to be deleted
    */
    {
        char a[RBUFFER+MIMETYPELEN+16]; //Buffer for if-none-match from header
        //Check if the resource cached in the client is the same
        if (check_etag(connection_prop,&a[0])==0) return 0;
    }


    //Tries to send the item from the cache
    if (cache_send_item(0,connection_prop)) return 0;


    int pagelen;
    bool parent;

    /*
    Determines if has to show the link to parent dir or not.
    If page is the basedir, it won't show the link to ..
    */
    {
        size_t basedir_len=strlen(real_basedir);
        if (connection_prop->strfile_len-1==basedir_len || connection_prop->strfile_len==basedir_len)
            parent=false;
        else
            parent=true;
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

        /*WARNING using the directory's mtime here allows better caching and
        the mtime will anyway be changed when files are added or deleted.

        Anyway i couldn't find the proof that it is changed also when files
        are modified.
        I tried on reiserfs and the directory's mtime changes too but i didn't
        find any doc about the other filesystems and OS.
        */
        send_http_header(200,pagelen,NULL,true,connection_prop->strfile_stat.st_mtime,connection_prop);
        write(sock,html,pagelen);

        //Write item in cache
        cache_store_item(0,connection_prop,html,pagelen);
    }

    free(html);//Frees the memory used for the page

    return 0;
}

/**
Writes a file to the socket, compressing it with gzip.
Then, since it is not possible to know the size of the compressed file,
it is not possible to send the size in advance, so it will set keep_alive to false
(and will seand the header "Connection: close" before) after sending.

sock is the socket
*/
#ifdef __COMPRESSION
static inline int write_compressed_file(connection_t* connection_prop ) {
    int sock=connection_prop->sock;

    if (
        connection_prop->strfile_stat.st_size>SIZE_COMPRESS_MIN &&
        connection_prop->strfile_stat.st_size<SIZE_COMPRESS_MAX
    ) { //Using compressed file method instead of sending it raw
        char *accept;
        char *end;

        if ((accept=strstr(connection_prop->http_param,"Accept-Encoding:"))!=NULL && (end=strstr(accept,"\r\n"))!=NULL) {

            //Avoid to parse the entire header.
            end[0]='\0';
            char* gzip=strstr(accept,"gzip");
            end[0]='\r';

            if (gzip==NULL) {
                return NO_ACTION;
            }
        }
    } else { //File size is not in the size range to be compressed
        return NO_ACTION;
    }


    connection_prop->keep_alive=false;
    send_http_header(200,
                     connection_prop->strfile_stat.st_size,
                     "Content-Encoding: gzip\r\n",
                     false,
                     connection_prop->strfile_stat.st_mtime,
                     connection_prop);
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
    return 0;
}
#endif


/**
 * Returns the amount of bytes to send
 * Collaterally, this function seeks the file to the requested position
 * finds out the mimetype
 * sends the header
 *
 * a must be a pointer to a buffer large at least RBUFFER+MIMETYPELEN+16
 * */
static inline unsigned long long int bytes_to_send(connection_t* connection_prop,char *a) {
    int http_code=200;
    errno=0;
    unsigned long long int count;
    char *hbuf=a;
    int remain=RBUFFER+MIMETYPELEN+16, t;
    a[0]='\0';

#ifdef __RANGE
    char *range="Range";
    char *if_range="If-Range";

    bool range_header=get_param_value(connection_prop->http_param,range,a,RBUFFER,strlen(range));
    time_t etag=connection_prop->strfile_stat.st_mtime;

    //Range header present, seeking for If-Range
    if (range_header) {
        char b[RBUFFER]; //Buffer for If-Range
        if (get_param_value(connection_prop->http_param,if_range,&b[0],sizeof(b),strlen(if_range))) {
            etag=(time_t)strtol(&b[1],NULL,0);
        }
    }

    /*
     * If range header is present and (If-Range has the same etag OR there is no If-Range)
     * */
    if (range_header && connection_prop->strfile_stat.st_mtime==etag) {//Find if it is a range request 5 is strlen of "range"

        unsigned long long int from,to;

        {
            //Locating from and to
            //Range: bytes=12323-123401
            char *eq, *sep;

            if ((eq=strstr(a,"="))==NULL ||(sep=strstr(eq,"-"))==NULL) {//Invalid data in Range header.
                errno =ERR_NOTHTTP;
                return ERR_NOTHTTP;
            }
            sep[0]=0;
            from=strtoll(eq+1,NULL,0);
            to=strtoll(sep+1,NULL,0);
        }

        if (to==0) { //If no to is specified, it is to the end of the file
            to=connection_prop->strfile_stat.st_size-1;
        }

        http_code=206;

        t=snprintf(hbuf,remain,"Accept-Ranges: bytes\r\nContent-Range: bytes %llu-%llu/%lld\r\n",(unsigned long long int)from,(unsigned long long int)to,(long long int)connection_prop->strfile_stat.st_size);
        hbuf+=t;
        remain-=t;

        lseek(connection_prop->strfile_fd,from,SEEK_SET);
        count=to-from+1;

    } else //Normal request
#endif
    {
        a[0]=0; //Reset buffer in case it isn't used for headers
        count=connection_prop->strfile_stat.st_size;
    }


    //Sending MIME to the client
#ifdef SEND_MIMETYPES
    if (weborf_conf.send_content_type) {
        thread_prop_t *thread_prop = pthread_getspecific(thread_key);
        const char* mime=mime_get_fd(thread_prop->mime_token,connection_prop->strfile_fd,&(connection_prop->strfile_stat));

        //t=
        snprintf(hbuf,remain,"Content-Type: %s\r\n",mime);
        //hbuf+=t;
        //remain-=t;
    }
#endif

    send_http_header(http_code,count,a,true,connection_prop->strfile_stat.st_mtime,connection_prop);
    return count;
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

If the file is larger, it will be sent using write_compressed_file,
see that function for details.

To work connection_prop.strfile_fd and connection_prop.strfile_stat must be set.
The file sent is the one specified by strfile_fd, and it will not be closed after.
*/
int write_file(connection_t* connection_prop) {

    int sock=connection_prop->sock;

    char a[RBUFFER+MIMETYPELEN+16]; //Buffer for Range, Content-Range headers, and reading if-none-match from header

    //Check if the resource cached in the client is the same
    if (check_etag(connection_prop,&a[0])==0) return 0;

#ifdef __COMPRESSION
    {
        //Tryies gzipping and sending the file
        int c= write_compressed_file(connection_prop);
        if (c!=NO_ACTION) return c;
    }
#endif

    //Determines how many bytes send, depending on file size and ranges
    //Also sends the http header
    unsigned long long int count= bytes_to_send(connection_prop,&a[0]);
    /*if (errno !=0) {
        int e=errno;
        errno=0;
        return e;
    }*/

    //Copy file using descriptors; from to and size
    return fd_copy(connection_prop->strfile_fd,sock,count);
}

/**
Sends a request for authentication
*/
int request_auth(connection_t *connection_prop) {
    int sock=connection_prop->sock;
    char * descr = connection_prop->page;
    connection_prop->status_code=401;

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
    int head_len = snprintf(head,HEADBUF,"HTTP/1.1 401 Authorization Required\r\nServer: " SIGNATURE "\r\nContent-Length: %d\r\nWWW-Authenticate: Basic realm=\"%s\"\r\n\r\n",page_len,descr);

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
int send_err(connection_t *connection_prop,int err,char* descr) {
    int sock=connection_prop->sock;

    connection_prop->status_code=err; //Sets status code, for the logs

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
    int head_len = snprintf(head,HEADBUF,"HTTP/1.1 %d %s\r\nServer: " SIGNATURE "\r\nContent-Length: %d\r\nContent-Type: text/html\r\n\r\n",err,descr ,(int)page_len);

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
This function reads post data and returns the pointer to the buffer containing the data (if there is any)
or NULL if there was no data.
If it doesn't return a null value, the returned pointer must be freed.
*/
string_t read_post_data(connection_t* connection_prop,buffered_read_t* read_b) {
    int sock=connection_prop->sock;
    string_t res;
    res.len=0;
    res.data=NULL;

    //Buffer for field's value
    char a[NBUFFER];
    //Gets the value
    char *content_length="Content-Length";
    bool r=get_param_value(connection_prop->http_param,content_length, a,NBUFFER,strlen(content_length));

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
    if (weborf_conf.virtual_host==false) return weborf_conf.basedir;

    char* result;
    char* h=strstr(http_param,"\r\nHost: ");

    if (h==NULL) return weborf_conf.basedir;

    h+=8;//Removing "Host:" string
    char* end=strstr(h,"\r");
    if (end==NULL) return weborf_conf.basedir;
    end[0]=0;
    result=getenv(h);
    end[0]='\r';

    if (result==NULL) return weborf_conf.basedir; //Reqeusted host doesn't exist

    return result;
}


/**
This function returns the reason phrase according to the response
code.
*/
static inline char *reason_phrase(int code) {
    code=code/100;

    switch (code) {
    case 2:
        return "OK";
    case 3:
        return "Found";
    case 4:
        return "Request error";
    case 5:
        return "Server error";
    case 1:
        return "Received";

    };
    return "Something is very wrong";
}

/**
This function sends a code header to the specified socket
size is the Content-Length field.
headers can be NULL or some extra headers to add. Headers must be
separated by \r\n and must have an \r\n at the end.

Content says if the size is for content-lenght or for entity-length

Timestamp is the timestamp for the content. Set to -1 to use the current
timestamp for Last-Modified and to omit ETag.

This function will automatically take care of generating Connection header when
needed, according to keep_alive and protocol_version of connection_prop

*/
int send_http_header(int code, unsigned long long int size,char* headers,bool content,time_t timestamp,connection_t* connection_prop) {
    int sock=connection_prop->sock;
    int len_head,wrote;
    char *head=malloc(HEADBUF);
    char* h_ptr=head;
    int left_head=HEADBUF;

    connection_prop->status_code=code; //Sets status code, for the logs

    if (head==NULL) {
#ifdef SERVERDBG
        syslog(LOG_CRIT,"Not enough memory to allocate buffers");
#endif
        return ERR_NOMEM;
    }
    if (headers==NULL) headers="";

    /*Defines the Connection header
    It will send the connection header if the setting is non-default
    Ie: will send keep alive if keep-alive is enabled and protocol is not 1.1
    And will send close if keep-alive isn't enabled and protocol is 1.1
    */
    char *connection_header;
    if (connection_prop->protocol_version!=HTTP_1_1 && connection_prop->keep_alive==true) {
        connection_header="Connection: Keep-Alive\r\n";
    } else if (connection_prop->protocol_version==HTTP_1_1 && connection_prop->keep_alive==false) {
        connection_header="Connection: close\r\n";
    } else {
        connection_header="";
    }

    len_head=snprintf(head,HEADBUF,"HTTP/1.1 %d %s\r\nServer: " SIGNATURE "\r\n%s",code,reason_phrase(code),connection_header);

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

    if (size>0 || (connection_prop->keep_alive==true)) {
        //Content length (or entity lenght) and extra headers
        if (content) {
            len_head=snprintf(head,left_head,"Content-Length: %llu\r\n",(unsigned long long int)size);
        } else {
            len_head=snprintf(head,left_head,"entity-length: %llu\r\n",(unsigned long long int)size);
        }

        head+=len_head;
        left_head-=len_head;
    }

    len_head=snprintf(head,left_head,"%s\r\n",headers);
    //head+=len_head; Not necessary because the snprintf was the last one
    left_head-=len_head;

    wrote=write (sock,h_ptr,HEADBUF-left_head);
    free(h_ptr);
    if (wrote!=HEADBUF-left_head) return ERR_BRKPIPE;
    return 0;
}

/**
This function writes on the specified socket a tar.gz file containing the required
directory
*/
static int tar_send_dir(connection_t* connection_prop) {

    /*
    WARNING
    This code checks the ETag and returns if the client has a copy in cache
    since the impact of using ETag for generated directory list is not known
    yet, if ETag goes away, also the following block will have to be deleted
    */
    {
        char a[RBUFFER+MIMETYPELEN+16]; //Buffer for if-none-match from header
        //Check if the resource cached in the client is the same
        if (check_etag(connection_prop,&a[0])==0) return 0;
    }
    
    
    connection_prop->keep_alive=false;
    
    send_http_header(200,
                     0,
                     "Content-Type: application/x-gzip\r\n",
                     true,
                     connection_prop->strfile_stat.st_mtime,
                     connection_prop);
    
    
    int pid=fork();
    
    if (pid==0) { //child, executing tar
        fclose (stdout); //Closing the stdout
        dup(connection_prop->sock); //Redirects the stdout
        nice(1); //Reducing priority
        execlp("tar","tar","-cz",connection_prop->strfile,(char *)0);
    } else if (pid>0) { //Father, does nothing
        int status;
        waitpid(pid,&status,0);
    } else { //Error
        return ERR_NOMEM; //Well not enough memory in process table...
    }
    return 0;
}


/**
Function executed when weborf is called from inetd
will use 0 as socket and exit after.
It is almost a copy of instance()
*/
void inetd() {
    
    thread_prop_t thread_prop;  //Server's props
    int bufFull=0;                                  //Amount of buf used
    connection_t connection_prop;                   //Struct to contain properties of the connection
    buffered_read_t read_b;                         //Buffer for buffered reader
    int sock=connection_prop.sock=0;                //Socket with the client,using normal file descriptor 0
    char * buf=calloc(INBUFFER+1,sizeof(char));     //Buffer to contain the HTTP request
    connection_prop.strfile=malloc(URI_LEN);        //buffer for filename

    thread_prop.id=0;
    signal(SIGPIPE, SIG_IGN);//Ignores SIGPIPE

    pthread_setspecific(thread_key, (void *)&thread_prop); //Set thread_prop as thread variable

#ifdef THREADDBG
    syslog(LOG_DEBUG,"Starting from inetd");
#endif

#ifdef IPV6
    struct sockaddr_in6 addr;//Local and remote address
    socklen_t addr_l=sizeof(struct sockaddr_in6);
#else
    struct sockaddr_in addr;
    int addr_l=sizeof(struct sockaddr_in);
#endif

    if (mime_init(&thread_prop.mime_token)!=0 || buffer_init(&read_b,BUFFERED_READER_SIZE)!=0 || buf==NULL || connection_prop.strfile==NULL) { //Unable to allocate the buffer
#ifdef SERVERDBG
        syslog(LOG_CRIT,"Not enough memory to allocate buffers for new thread");
#endif
        goto release_resources;
    }

    //Converting address to string
#ifdef IPV6
    getpeername(sock, (struct sockaddr *)&addr, &addr_l);
    inet_ntop(AF_INET6, &addr.sin6_addr, connection_prop.ip_addr, INET6_ADDRSTRLEN);
#else
    getpeername(sock, (struct sockaddr *)&addr,(socklen_t *) &addr_l);
    inet_ntop(AF_INET, &addr.sin_addr, connection_prop.ip_addr, INET_ADDRSTRLEN);
#endif

    handle_requests(buf,&read_b,&bufFull,&connection_prop,thread_prop.id);
    //close(sock);//Closing the socket
    //buffer_reset (&read_b);

release_resources:
    exit(0);
    /* No need to free memory and resources
    free(buf);
    free(connection_prop.strfile);
    buffer_free(&read_b);
    mime_release(thread_prop.mime_token);
    */
    return;
}
