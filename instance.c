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
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "auth.h"
#include "cachedir.h"
#include "cgi.h"
#include "dict.h"
#include "http.h"
#include "instance.h"
#include "mime.h"
#include "myio.h"
#include "mynet.h"
#include "mystring.h"
#include "queue.h"
#include "types.h"
#include "utils.h"

extern syn_queue_t queue;                   //Queue for open sockets

extern t_thread_info thread_info;

extern weborf_configuration_t weborf_conf;

extern char* indexes[MAXINDEXCOUNT];        //Array containing index files
extern int indexes_l;                       //Length of array
extern pthread_key_t thread_key;            //key for pthread_setspecific


int request_auth(connection_t *connection_prop);
static void write_dir(char *real_basedir, connection_t * connection_prop);
void read_post_data(connection_t * connection_prop);
static int serve_request(connection_t* connection_prop);
static int send_error_header(connection_t *connection_prop);
static void get_or_post(connection_t *connection_prop);
static inline void read_req_headers(connection_t* connection_prop);
static inline void handle_request(connection_t* connection_prop);

static void prepare_put(connection_t *connection_prop);
static void do_put(connection_t* connection_prop);
static void do_get_file(connection_t* connection_prop);
static void do_copy_from_post_to_socket(connection_t* connection_prop);

static void prepare_tar_send_dir(connection_t* connection_prop);
static void do_tar_send_dir(connection_t* connection_prop);


/**
 * TODO
 **/
static inline void handle_request(connection_t* connection_prop) {

    ssize_t r;

    while(true) {
        switch (connection_prop->status) {
        case STATUS_CHECK_AUTH:
            // -> STATUS_READY_TO_SEND
            if (auth_check_request(connection_prop)!=0) { //If auth is required
                connection_prop->response.status_code = HTTP_CODE_UNAUTHORIZED;
                connection_prop->status = STATUS_ERR;
            } else {
                connection_prop->status = STATUS_READY_TO_SEND;
            }
            break;
        case STATUS_WAIT_DATA:
            r=buffer_fill(connection_prop->sock,&(connection_prop->read_b));
            if (r==0)
                connection_prop->status=STATUS_END;
            else
                connection_prop->status = connection_prop->status_next;

            break;
        case STATUS_WAIT_HEADER:
            // -> STATUS_CHECK_AUTHs
            read_req_headers(connection_prop);
            break;
        case STATUS_PAGE_SENT:
#ifdef REQUESTDBG
            syslog(LOG_INFO,
                   "%s - %d - %s %s",
                   connection_prop->ip_addr,
                   connection_prop->response.status_code,
                   connection_prop->method,
                   connection_prop->page);
#endif
            connection_prop->status=connection_prop->response.keep_alive ? STATUS_READY_FOR_NEXT: STATUS_END;

            free(connection_prop->post_data.data);
            connection_prop->post_data.data=NULL;
            if (connection_prop->strfile_fd>=0) close (connection_prop->strfile_fd);


            break;
        case STATUS_READY_TO_SEND:
            if (connection_prop->request.method_id == POST || connection_prop->request.method_id == PROPFIND) {
                read_post_data(connection_prop); //TODO read this data iteratively
            }
            connection_prop->status = STATUS_SERVE_REQUEST;
            break;
        case STATUS_PUT_METHOD:
            do_put(connection_prop);
            break;
        case STATUS_GET_METHOD:
            do_get_file(connection_prop);
            break;
        case STATUS_TAR_DIRECTORY:
            do_tar_send_dir(connection_prop);
            break;
        case STATUS_COPY_FROM_POST_DATA_TO_SOCKET:
            // -> STATUS_PAGE_SENT || STATUS_ERR_NO_CONNECTION
            do_copy_from_post_to_socket(connection_prop);
            break;
        case STATUS_SERVE_REQUEST:
            serve_request(connection_prop);
            break;
        case STATUS_READY_FOR_NEXT:
            net_sock_flush(connection_prop->sock);
            memset(
                connection_prop->buf.data,
                0,
                connection_prop->buf.len);
            connection_prop->buf.len=0;

            connection_prop->status=STATUS_WAIT_HEADER;
            break;
        case STATUS_ERR:
            connection_prop->status = STATUS_PAGE_SENT;
            send_error_header(connection_prop);
            break;
        case STATUS_SEND_HEADERS:
            connection_prop->status = connection_prop->status_next;
            send_error_header(connection_prop);
            break;
        case STATUS_ERR_NO_CONNECTION:
#ifdef REQUESTDBG
            syslog(LOG_INFO,"%s - FAILED - %s %s",connection_prop->ip_addr,connection_prop->method,connection_prop->page);
#endif

        case STATUS_END:
            connection_prop->status=STATUS_NONE;
            return;
            break;

        };
    }

}

static inline void read_req_headers(connection_t* connection_prop) {
    char *buf=connection_prop->buf.data;
    size_t *buf_len = &(connection_prop->buf.len);
    buffered_read_t* read_b = & connection_prop->read_b;

    int sock=connection_prop->sock;

    size_t r;//Read bytes
    char *end;//Pointer to header's end
    //18 is the size of the shortest possible HTTP request
    if (*buf_len<18) {
        r=buffer_read_non_fill(sock, buf+(*buf_len),18-*buf_len,read_b);
        if (r<=0) goto more_data;
        (*buf_len)+=r;//Sets the end of the user buffer (may contain more than one header)
    }

    while (strstr(buf+(*buf_len)-4,"\r\n\r")==NULL) { //Determines if there is a \r\n\r which is an ending sequence
        r=buffer_read_non_fill(sock, buf+(*buf_len),2,read_b);
        if (r<=0) goto more_data;

        (*buf_len)+=r;//Sets the end of the user buffer (may contain more than one header)

        //Buffer full and still no valid http header
        if ((*buf_len)>=INBUFFER)
            goto bad_request;
    }

    if ((end=strstr(buf+(*buf_len)-4,"\r\n\r\n"))==NULL) {//If we didn't read yet the lst \n of the ending sequence, we read it now, so it won't disturb the next request
        r=buffer_read_non_fill(sock, buf+*buf_len,1,read_b);//Reads 1 char and adds to the buffer
        if (r<=0) goto more_data;
        (*buf_len)++;

        if (buf[*buf_len-1]!='\n')
            goto bad_request;

        end=&(buf[*buf_len-4]);
    }

    end[2]='\0';//Terminates the header, leaving a final \r\n in it

    if (http_set_connection_t(buf,connection_prop)==-1) goto bad_request;

#ifdef THREADDBG
    syslog(LOG_INFO,"Requested page: %s to Thread %ld",connection_prop->page,id);
#endif

    connection_prop->status = STATUS_CHECK_AUTH;
    return;

bad_request:
    connection_prop->response.status_code = HTTP_CODE_BAD_REQUEST;
    connection_prop->status=STATUS_ERR;
    connection_prop->response.keep_alive=false;
    return;
more_data:
    connection_prop->status_next = STATUS_WAIT_HEADER;
    connection_prop->status = STATUS_WAIT_DATA;
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

static inline int thr_init_connect_prop(connection_t *connection_prop) {
    connection_prop->buf.data = calloc(INBUFFER+1,sizeof(char));
    connection_prop->buf.len = 0;

    connection_prop->response.headers.data = malloc(HEADBUF);
    connection_prop->response.headers.len = 0;

    connection_prop->strfile=malloc(URI_LEN);    //buffer for filename

    return buffer_init(&(connection_prop->read_b),BUFFERED_READER_SIZE)!=0 ||
           connection_prop->buf.data==NULL ||
           connection_prop->response.headers.data == NULL ||
           connection_prop->strfile==NULL;

}

static inline void thr_free_connect_prop(connection_t *connection_prop) {
    free(connection_prop->buf.data);
    free(connection_prop->strfile);
    free(connection_prop->response.headers.data);
    buffer_free(&(connection_prop->read_b));
}

/**
Function executed at the beginning of the thread
Takes open sockets from the queue and serves the requests
Doesn't do busy waiting
*/
void * instance(void * nulla) {
    thread_prop_t thread_prop;  //Server's props
    pthread_setspecific(thread_key, (void *)&thread_prop); //Set thread_prop as thread variable
    connection_t connection_prop;                   //Struct to contain properties of the connection

    //General init of the thread
    thread_prop.id=(long int)nulla;//Set thread's id
#ifdef THREADDBG
    syslog(LOG_DEBUG,"Starting thread %ld",thread_prop.id);
#endif

    if (mime_init(&thread_prop.mime_token)!=0 || thr_init_connect_prop(&connection_prop)) { //Unable to allocate the buffer
#ifdef SERVERDBG
        syslog(LOG_CRIT,"Not enough memory to allocate buffers for new thread");
#endif
        goto release_resources;
    }

    //Start accepting sockets
    change_free_thread(thread_prop.id,1,0);

    while (true) {
        q_get(&queue, &(connection_prop.sock));//Gets a socket from the queue
        change_free_thread(thread_prop.id,-1,0);//Sets this thread as busy

        if (connection_prop.sock<0) { //Was not a socket but a termination order
            goto release_resources;
        }

        //Converting address to string
        net_getpeername(connection_prop.sock,connection_prop.ip_addr);

#ifdef THREADDBG
        syslog(LOG_DEBUG,"Thread %ld: Reading from socket",thread_prop.id);
#endif
        connection_prop.status=STATUS_READY_FOR_NEXT;
        handle_request(&connection_prop);

#ifdef THREADDBG
        syslog(LOG_DEBUG,"Thread %ld: Closing socket with client",thread_prop.id);
#endif

        close(connection_prop.sock);//Closing the socket
        buffer_reset (&(connection_prop.read_b));

        change_free_thread(thread_prop.id,1,0);//Sets this thread as free
    }


release_resources:
#ifdef THREADDBG
    syslog(LOG_DEBUG,"Terminating thread %ld",thread_prop.id);
#endif
    thr_free_connect_prop(&connection_prop);
    mime_release(thread_prop.mime_token);
    change_free_thread(thread_prop.id,0,-1);//Reduces count of threads
    pthread_exit(0);
    return NULL;//Never reached
}


/**
 * Prepares for PUT method, reading the headers, opening the file descriptors
 **/
static void prepare_put(connection_t *connection_prop) {
    if (weborf_conf.authsock==NULL) {
        connection_prop->response.status_code= HTTP_CODE_FORBIDDEN;
        return;
    }

    //Checking if there is any unsupported Content-* header. In this case return 501 (Not implemented)
    {
        char*header=connection_prop->http_param;

        char *content_length="Content-Length";
        while ((header=strstr(header,"Content-"))!=NULL) {
            if (strncmp(header,content_length,strlen(content_length))!=0) {
                connection_prop->response.status_code = HTTP_CODE_NOT_IMPLEMENTED;
                return;
            }
            header++;
        }
    }

    ssize_t content_length = http_read_content_length(connection_prop);

    if (content_length==-1) { //Error if there is no content-length header
        connection_prop->response.status_code = HTTP_CODE_BAD_REQUEST;
        return;
    }
    connection_prop->response.size =  content_length;

    //Checks if file already exists or not (needed for response code)
    if (access(connection_prop->strfile,R_OK | W_OK)==0) {//Resource already existed (No content)
        connection_prop->response.status_code = HTTP_CODE_OK_NO_CONTENT;
    } else {//Resource was created (Created)
        connection_prop->response.status_code = HTTP_CODE_OK_CREATED;
    }

    connection_prop->strfile_fd = open(connection_prop->strfile,O_WRONLY|O_CREAT,S_IRUSR|S_IWUSR);
    if (connection_prop->strfile_fd<0) {
        connection_prop->response.status_code = HTTP_CODE_PAGE_NOT_FOUND;
        return;
    }

    //TODO check for errors
    ftruncate(connection_prop->strfile_fd,content_length);

    connection_prop->status = STATUS_PUT_METHOD;

    return;
}

/**
This function handles a PUT request.

It requires the socket, connection_t struct and buffered_read_t to read from
the socket, since the reading is internally buffered.

PUT size has no hardcoded limits.
Auth provider has to check for the file's size and refuse it if it is the case.
This function will not work if there is no auth provider.
*/
static void do_put(connection_t* connection_prop) {

    ssize_t written = buffer_flush_fd(connection_prop->strfile_fd,&connection_prop->read_b,connection_prop->response.size);
    connection_prop->response.size -= written;

    if (connection_prop->response.size!=0) {
        connection_prop->status = STATUS_WAIT_DATA;
        connection_prop->status_next = STATUS_PUT_METHOD;
    } else {
        connection_prop->status = STATUS_ERR;
    }

    return;
}

/**
This function handles a DELETE request.

It requires the socket, connection_t struct

Auth provider has to check if it is allowed to delete the file or not.
This function will not work if there is no auth provider.
*/
int delete_file(connection_t* connection_prop) {
    connection_prop->status = STATUS_ERR;
    int retval;

    if (weborf_conf.authsock==NULL) {
        connection_prop->response.status_code = HTTP_CODE_FORBIDDEN;
        return -1;
    }

    //connection_prop->strfile
    struct stat stat_d;
    if (stat(connection_prop->strfile,&stat_d)<0) {
        connection_prop->response.status_code = HTTP_CODE_PAGE_NOT_FOUND;
        return -1;
    }

    if (S_ISDIR(stat_d.st_mode)) {
        retval=dir_remove(connection_prop->strfile);
    } else {
        retval=unlink(connection_prop->strfile);
    }

    if (retval!=0) {//Returns a generic error
        connection_prop->response.status_code = HTTP_CODE_FORBIDDEN;
        return -1;
    }
    connection_prop->response.status_code = HTTP_CODE_OK_NO_CONTENT;
    return 0;
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
    connection_prop->response.status_code = HTTP_CODE_OK;
    http_append_header(connection_prop,ALLOWED);
    connection_prop->status = STATUS_ERR;
    return 0;
}

/**
 * TODO
 **/
static int serve_request(connection_t* connection_prop) {
    bool header_sent=true;

    switch (connection_prop->request.method_id) {
    case POST:

    case GET:
        get_or_post(connection_prop);
        return 0;
    case PUT:
        prepare_put(connection_prop);
        return 0;
    case DELETE:
        delete_file(connection_prop);
        return 0;
    case OPTIONS:
        options(connection_prop);
        return 0;
#ifdef WEBDAV
    case PROPFIND:
        header_sent=propfind(connection_prop);
        break;
    case MKCOL:
        mkcol(connection_prop);
        return 0;
    case COPY:
    case MOVE:
        copy_move(connection_prop);
        return 0;
#endif
    }

    if (!header_sent) {
        connection_prop->status = STATUS_ERR;
    } else {
        connection_prop->status= STATUS_PAGE_SENT;
    }
    return 0;
}

/**
 * With get or post this function is called, it decides if execute CGI or send
 * the simple file, and if the request points to a directory it will redirect to
 * the appropriate index file or show the list of the files.
 *
 * RETURNS:
 *
 * true: if an header was sent
 * false: otherwise
 * */
static void get_or_post(connection_t *connection_prop) {
    if ((connection_prop->strfile_fd=open(connection_prop->strfile,O_RDONLY))<0) {
        //File doesn't exist. Must return errorcode
        connection_prop->response.status_code = HTTP_CODE_PAGE_NOT_FOUND;
        connection_prop->status = STATUS_ERR;
        return;
    }

    //TODO: check fstat result
    fstat(connection_prop->strfile_fd, &connection_prop->strfile_stat);


    if (S_ISDIR(connection_prop->strfile_stat.st_mode)) {//Requested a directory

        if (!endsWith(connection_prop->strfile,"/",connection_prop->strfile_len,1)) {//Putting the ending / and redirect
            http_append_header_str(connection_prop,"Location: %s/\r\n",connection_prop->page);
            connection_prop->response.status_code= HTTP_CODE_MOVED_PERMANENTLY;
            connection_prop->status = STATUS_ERR;
            return;
        }

        if (weborf_conf.tar_directory) {
            prepare_tar_send_dir(connection_prop);
            return;
        }

        //TODO:refactory into two functions
        //Cycling index files
        char* index_name=&connection_prop->strfile[connection_prop->strfile_len];//Pointer to where to write the filename
        unsigned int i;

        //Cyclyng through the indexes
        for (i=0; i<weborf_conf.indexes.len ; i++) {
            
            if (URI_LEN-connection_prop->strfile_len-1 > weborf_conf.indexes.data_l[i]) {
                memcpy(index_name,weborf_conf.indexes.data[i],weborf_conf.indexes.data_l[i]+1);
            } else {
                //TODO log the error for the too long path
                continue;
            }
            
            
            if (access(connection_prop->strfile,R_OK)==0) { //If index exists, redirect to it
                http_append_header_str_str(connection_prop,"Location: %s%s\r\n",connection_prop->page,weborf_conf.indexes.data[i]);
                connection_prop->response.status_code=HTTP_CODE_SEE_OTHER;
                connection_prop->status = STATUS_ERR;
                return;
            }
        }

        connection_prop->strfile[connection_prop->strfile_len]=0; //Removing the index part
        write_dir(connection_prop->basedir,connection_prop);
        return;

    } else {//Requested an existing file
        if (weborf_conf.exec_script) { //Scripts enabled
            //FIXME: refactory CGI
            size_t q_;
            int f_len;
            for (q_=0; q_<weborf_conf.cgi_paths.len; q_+=2) { //Check if it is a CGI script
                f_len=weborf_conf.cgi_paths.data_l[q_];
                if (endsWith(connection_prop->page+connection_prop->page_len-f_len,weborf_conf.cgi_paths.data[q_],f_len,f_len)) {
                    return exec_page(weborf_conf.cgi_paths.data[++q_],connection_prop);
                }
            }
        }

        //send normal file, control reaches this point if scripts are disabled or if the filename doesn't trigger CGI
        prepare_get_file(connection_prop);
    }
}

/**
 * If retval is not 0, this function will send an appropriate error
 * or request authentication or any other header that does not imply
 * sending some content as well.
 * */
static int send_error_header(connection_t *connection_prop) {
    switch (connection_prop->response.status_code) {
    case 0:
        return 0;
    case HTTP_CODE_INTERNAL_SERVER_ERROR:
        return send_err(connection_prop,500,"Internal server error");
    case HTTP_CODE_PAGE_NOT_FOUND:
        return send_err(connection_prop,404,"Page not found");
    case HTTP_CODE_SERVICE_UNAVAILABLE:
        return send_err(connection_prop,503,"Service Unavailable");
    case HTTP_CODE_BAD_REQUEST:
        return send_err(connection_prop,400,"Bad request");
    case HTTP_CODE_FORBIDDEN:
        return send_err(connection_prop,403,"Forbidden");
    case HTTP_CODE_NOT_IMPLEMENTED:
        return send_err(connection_prop,501,"Not implemented");
    case HTTP_CODE_PRECONDITION_FAILED:
        return send_err(connection_prop,412,"Precondition Failed");
    case HTTP_CODE_CONFLICT:
        return send_err(connection_prop,409,"Conflict");
    case HTTP_CODE_INSUFFICIENT_STORAGE:
        return send_err(connection_prop,507,"Insufficient Storage");
    case HTTP_CODE_METHOD_NOT_ALLOWED:
        return send_err(connection_prop,405,"Method Not Allowed");
    case HTTP_CODE_UNAUTHORIZED:
        return request_auth(connection_prop);
    default:
        //case HTTP_CODE_OK_NO_CONTENT:
        //    return send_http_header(connection_prop);
        //case HTTP_CODE_OK_CREATED:
        //    return send_http_header(connection_prop);
        //default:
        return send_http_header(connection_prop);
    }
    return 0; //Make gcc happy

}

/**
 * This function creates a directory listing and stores it in the POST data buffer,
 * then the status is set to:
 * connection_prop->status = STATUS_SEND_HEADERS;
 * connection_prop->status_next = STATUS_COPY_FROM_POST_DATA_TO_SOCKET;
 * 
 * In case of errors status is set accordingly.
 **/
static void write_dir(char* real_basedir,connection_t* connection_prop) {
    /*
    WARNING
    This code checks the ETag and returns if the client has a copy in cache
    since the impact of using ETag for generated directory list is not known
    yet, if ETag goes away, also the following block will have to be deleted
    */
    time_t etag = http_read_if_none_match(connection_prop);
    if (connection_prop->strfile_stat.st_mtime==etag) {
        //Browser has the item in its cache
        connection_prop->response.status_code = HTTP_CODE_NOT_MODIFIED;
        connection_prop->response.timestamp=etag;
        connection_prop->status = STATUS_ERR;
        return;
    }

    //Tries to send the item from the cache
    if (cache_send_item(0,connection_prop)) return;

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

    free(connection_prop->post_data.data);

    connection_prop->post_data.data=malloc(MAXSCRIPTOUT);//Memory for the html page
    if (connection_prop->post_data.data==NULL) { //No memory
#ifdef SERVERDBG
        syslog(LOG_CRIT,"Not enough memory to allocate buffers to list directory");
#endif
        connection_prop->response.status_code = HTTP_CODE_SERVICE_UNAVAILABLE;
        connection_prop->status = STATUS_ERR;
        return;
    }

    int r = list_dir (connection_prop,connection_prop->post_data.data,MAXSCRIPTOUT,parent);
    if (r<0) { //Creates the page
        connection_prop->response.status_code = HTTP_CODE_PAGE_NOT_FOUND;
        connection_prop->status = STATUS_ERR;
        return;
    } else { //If there are no errors sends the page
        connection_prop->bytes_to_copy = connection_prop->post_data.len = r;

        /*WARNING using the directory's mtime here allows better caching and
        the mtime will anyway be changed when files are added or deleted.

        Anyway it is not changed also when files are modified.
        */
        connection_prop->response.status_code = HTTP_CODE_OK;

        connection_prop->response.size = connection_prop->post_data.len;
        connection_prop->response.timestamp = connection_prop->strfile_stat.st_mtime;
        cache_store_item(0,connection_prop,connection_prop->post_data.data,connection_prop->post_data.len);

        connection_prop->status = STATUS_SEND_HEADERS;
        connection_prop->status_next = STATUS_COPY_FROM_POST_DATA_TO_SOCKET;

    }
}

/**
 * TODO:
 * */
void prepare_get_file(connection_t* connection_prop) {

    size_t count;

    time_t etag = http_read_if_none_match(connection_prop);
    if (connection_prop->strfile_stat.st_mtime==etag) {
        //Browser has the item in its cache
        connection_prop->response.status_code = HTTP_CODE_NOT_MODIFIED;
        connection_prop->response.timestamp=etag;
        connection_prop->status = STATUS_ERR;
        return;
    }

    connection_prop->response.status_code=HTTP_CODE_OK;

#ifdef __RANGE

    etag = http_read_if_range(connection_prop);
    if (etag==-1) etag = connection_prop->strfile_stat.st_mtime;

    size_t from,to;

    /*
     * If range header is present and (If-Range has the same etag OR there is no If-Range)
     * */
    if (http_read_range(connection_prop,&from,&to) && connection_prop->strfile_stat.st_mtime==etag) {//Find if it is a range request 5 is strlen of "range"

        if (to==0) { //If no to is specified, it is to the end of the file
            to=connection_prop->strfile_stat.st_size-1;
        }

        connection_prop->response.status_code=HTTP_CODE_PARTIAL_CONTENT;
        http_append_header_llu_llu_lld(connection_prop,"Content-Range: bytes %llu-%llu/%lld\r\n",
                                       (unsigned long long int)from,
                                       (unsigned long long int)to,
                                       (long long int)connection_prop->strfile_stat.st_size
                                      );

        lseek(connection_prop->strfile_fd,from,SEEK_SET);
        count=to-from+1;

    } else //Normal request
#endif
        count=connection_prop->strfile_stat.st_size;



#ifdef SEND_MIMETYPES
    //Sending MIME to the client
    if (weborf_conf.send_content_type) {
        thread_prop_t *thread_prop = pthread_getspecific(thread_key);
        const char* mime=mime_get_fd(thread_prop->mime_token,connection_prop->strfile_fd,&(connection_prop->strfile_stat));

        http_append_header_str(connection_prop,"Content-Type: %s\r\n",mime);
    }
#endif

    connection_prop->response.size=count;
    connection_prop->response.timestamp = connection_prop->strfile_stat.st_mtime;

    connection_prop->status = STATUS_SEND_HEADERS;
    connection_prop->status_next = STATUS_GET_METHOD;

    connection_prop->bytes_to_copy = count;
}

/**
 * TODO
 **/
static void do_get_file(connection_t* connection_prop) {

    if (connection_prop->bytes_to_copy > 0) {

        size_t count = connection_prop->bytes_to_copy > TCP_FRAME_SIZE ? TCP_FRAME_SIZE : connection_prop->bytes_to_copy;
        int r = fd_copy(connection_prop->strfile_fd,connection_prop->sock,count);

        if (r==0)
            connection_prop->bytes_to_copy-=count;
        else
            connection_prop->status = STATUS_ERR_NO_CONNECTION;
    }

    if (connection_prop->bytes_to_copy==0) {
        connection_prop->status = STATUS_PAGE_SENT;
    }

}

/**
 * TODO: write documentation
 **/
static void do_copy_from_post_to_socket(connection_t* connection_prop) {

    if (connection_prop->bytes_to_copy == 0 ) {
        connection_prop->status = STATUS_PAGE_SENT;
        return;
    }

    size_t count = connection_prop->bytes_to_copy > TCP_FRAME_SIZE ? TCP_FRAME_SIZE : connection_prop->bytes_to_copy;

    char* start=connection_prop->post_data.data + (connection_prop->post_data.len - connection_prop->bytes_to_copy);

    ssize_t r = write(connection_prop->sock,start,count);
    if (r>0) {
        connection_prop->bytes_to_copy -= r;
    } else {
        connection_prop->status = STATUS_ERR_NO_CONNECTION;
    }


}

/**
Sends a request for authentication
*/
int request_auth(connection_t *connection_prop) {
    int sock=connection_prop->sock;
    char * descr = connection_prop->page;
    connection_prop->response.status_code=HTTP_CODE_UNAUTHORIZED;

    char * page =  HTMLHEAD "<H1>Authorization required</H1><p>Authenticate.</p>" HTMLFOOT;

    dprintf(sock,"HTTP/1.1 401 Authorization Required\r\nServer: " SIGNATURE "\r\nContent-Length: %zu\r\nWWW-Authenticate: Basic realm=\"%s\"\r\n\r\n%s",strlen(page),descr,page);
    return 0;
}


/**
Sends an error to the client
*/
int send_err(connection_t *connection_prop,int err,char* descr) {
    int sock=connection_prop->sock;

    connection_prop->response.status_code=err; //Sets status code, for the logs

    //Buffer for both header and page
    char * head=malloc(MAXSCRIPTOUT+HEADBUF);

    if (head==NULL) {
#ifdef SERVERDBG
        syslog(LOG_CRIT,"Not enough memory to allocate buffers");
#endif
        return HTTP_CODE_SERVICE_UNAVAILABLE;
    }

    char * page=head+HEADBUF;

    //Prepares the page
    int page_len=snprintf(page,MAXSCRIPTOUT,"%s <H1>Error %d</H1>%s %s",HTMLHEAD,err,descr,HTMLFOOT);

    //Prepares the header
    int head_len = snprintf(head,HEADBUF,"HTTP/1.1 %d %s\r\nServer: " SIGNATURE "\r\nContent-Length: %d\r\nContent-Type: text/html\r\n\r\n",err,descr ,page_len);

    //Sends the http header
    if (write (sock,head,head_len)!=head_len) {
        free(head);
        return HTTP_CODE_DISCONNECTED;
    }

    //Sends the html page
    if (write(sock,page,page_len)!=page_len) {
        free(head);
        return HTTP_CODE_DISCONNECTED;
    }

    free(head);
    return 0;
}

/**
 * This function reads post data and sets it in connection_prop.
 * If there is too much data, it isn't read, and it will lead to
 * a failure in the next pipelined request.
 *
 * It allocates memory so the post_data.data must be freed
 **/
void read_post_data(connection_t* connection_prop) {
    buffered_read_t* read_b = & (connection_prop->read_b);
    int sock=connection_prop->sock;

    ssize_t content_length = http_read_content_length(connection_prop);

    //Post size is ok and buffer is allocated
    if (content_length>=0 && content_length<=POST_MAX_SIZE && (connection_prop->post_data.data=malloc(content_length))!=NULL) {
        connection_prop->post_data.len=buffer_read(sock,connection_prop->post_data.data,content_length,read_b);
    }

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
int send_http_header(connection_t* connection_prop) {
    int sock=connection_prop->sock;
    size_t size = connection_prop->response.size;
    time_t timestamp = connection_prop->response.timestamp;

    int len_head;
    int bsize = strlen(http_reason_phrase(0)) +
                strlen("Connection: Keep-Alive\r\n")+
                strlen("HTTP/1.1 %d %s\r\nServer: " SIGNATURE "\r\n%s") +
                strlen("Content-Length: %llu\r\n" "Accept-Ranges: bytes\r\n") +
                strlen("ETag: \"%d\"\r\n") +
                strlen("Last-Modified: %a, %d %b %Y %H:%M:%S GMT\r\n")+
                20; //this magic 20 is spcace for the numeric size
    char *head=malloc(bsize);
    char* h_ptr=head;
    int left_head=bsize;

    char *connection_header=""; //Contains the "Connection" header

    if (head==NULL) {
#ifdef SERVERDBG
        syslog(LOG_CRIT,"Not enough memory to allocate buffers");
#endif
        return HTTP_CODE_SERVICE_UNAVAILABLE;
    }

    /*Defines the Connection header
    It will send the connection header if the setting is non-default
    Ie: will send keep alive if keep-alive is enabled and protocol is not 1.1
    And will send close if keep-alive isn't enabled and protocol is 1.1
    */

    if (connection_prop->protocol_version!=HTTP_1_1 && connection_prop->response.keep_alive==true) {
        connection_header="Connection: Keep-Alive\r\n";
    } else if (connection_prop->protocol_version==HTTP_1_1 && connection_prop->response.keep_alive==false) {
        connection_header="Connection: close\r\n";
    }

    len_head=snprintf(
                 head,
                 HEADBUF,
                 "HTTP/1.1 %d %s\r\nServer: " SIGNATURE "\r\n%s",
                 connection_prop->response.status_code,
                 http_reason_phrase(connection_prop->response.status_code),
                 connection_header
             );
    //This stuff moves the pointer to the buffer forward, and reduces the left space in the buffer itself
    //Next snprintf will append their strings to the buffer, without overwriting.
    head+=len_head;
    left_head-=len_head;


    if (size>0 || (connection_prop->response.keep_alive==true)) {
        const char* length;
        if (connection_prop->response.size_type==LENGTH_CONTENT) {
            length="Content-Length: %zu\r\n"
#ifdef __RANGE
                   "Accept-Ranges: bytes\r\n"
#endif
                   ;

        } else {
            length = "entity-length: %zu\r\n";
        }

        len_head=snprintf(head,left_head,length ,size);
        head+=len_head;
        left_head-=len_head;
    }

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

    dprintf(sock,"%s%s\r\n",h_ptr, connection_prop->response.headers.data );
    free(h_ptr);
    return 0;
}

/**
This function writes on the specified socket a tar.gz file containing the required
directory.
No caching at all is used or is allowed to the clients

TODO: fix this
*/

static void prepare_tar_send_dir(connection_t* connection_prop) {
    connection_prop->response.keep_alive=false;

    //Last char is always '/', i null it so i can use default name
    connection_prop->strfile[--connection_prop->strfile_len]=0;

    char* dirname=strrchr(connection_prop->strfile,'/')+1;
    if (strlen(dirname)==0) dirname="directory";
    
    http_append_header(connection_prop,"Content-Type: application/x-gzip\r\n");
    http_append_header_str(connection_prop,"Content-Disposition: attachment; filename=\"%s.tar.gz\"\r\n",dirname);

    connection_prop->response.status_code=200;
    connection_prop->response.size_type=LENGTH_ENTITY;
    
    connection_prop->status = STATUS_SEND_HEADERS;
    connection_prop->status_next = STATUS_TAR_DIRECTORY;
}

static void do_tar_send_dir(connection_t* connection_prop) {
    
    
    int pid=detached_fork();

    if (pid==0) { //child, executing tar
        close(1);
        dup(connection_prop->sock); //Redirects the stdout
        nice(1); //Reducing priority
        execlp("tar","tar","-cz",connection_prop->strfile,(char *)0);
        return HTTP_CODE_SERVICE_UNAVAILABLE;
    } else { //Father, does nothing
        connection_prop->status = STATUS_PAGE_SENT;
    }

}


/**
Function executed when weborf is called from inetd
will use 0 as socket and exit after.
It is almost a copy of instance()
*/
void inetd() {
    thread_prop_t thread_prop;  //Server's props
    pthread_setspecific(thread_key, (void *)&thread_prop); //Set thread_prop as thread variable

    //General init of the thread
    thread_prop.id=0;
    connection_t connection_prop;                   //Struct to contain properties of the connection

    if (mime_init(&thread_prop.mime_token)!=0 || thr_init_connect_prop(&connection_prop)) { //Unable to allocate the buffer
#ifdef SERVERDBG
        syslog(LOG_CRIT,"Not enough memory to allocate buffers for new thread");
#endif
        goto release_resources;
    }

    connection_prop.sock=0;

    //Converting address to string
    net_getpeername(connection_prop.sock,connection_prop.ip_addr);

    connection_prop.status=STATUS_READY_FOR_NEXT;
    handle_request(&connection_prop);

    //close(connection_prop.sock);//Closing the socket
    //buffer_reset (&(connection_prop.read_b));


release_resources:
    exit(0);
}
