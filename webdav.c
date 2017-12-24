/*
Weborf
Copyright (C) 2009  Salvo "LtWorf" Tomaselli

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
*/

#include "options.h"

#ifdef WEBDAV

#include <pthread.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdio.h>

#include "webdav.h"
#include "instance.h"
#include "mime.h"
#include "myio.h"
#include "mystring.h"
#include "utils.h"
#include "cachedir.h"

typedef struct {
    bool getetag :1;
    bool getcontentlength :1;
    bool resourcetype :1;
    bool getlastmodified :1;
    bool getcontenttype :1;
    bool deep :1;
    unsigned type :2;
} t_dav_details;

typedef union {
    unsigned int int_version;
    t_dav_details dav_details;
} u_dav_details;


extern weborf_configuration_t weborf_conf;
extern pthread_key_t thread_key;            //key for pthread_setspecific


/**
This function will create a copy of the source URI into the
dest buffer %-escaping it.

dest_size specifies the size of the dest buffer
*/
static inline void escape_uri(char *source, char *dest, int dest_size) {
    int i;

    //dest_size must have at least 4 bytes to contain %00\0
    for (i=0; source[i]!=0 && dest_size>=4; i++) {

        //The or with the space changes it to lower case, so there is no need to compare with two ranges
        if (((source[i] | ' ')>='a' && (source[i] | ' ')<='z' ) || (source[i]>='-' && source[i]<='9')) {
            dest[0]=source[i];
            dest[1]='\0';
            dest++;
            dest_size--;
        } else {
            //Prints % followed by 2 digits hex code of the character
            sprintf(dest,"%%%02x",source[i]);
            dest+=3;
            dest_size-=3;
        }
    }
}


/**
This function will use the result param prop to return the required
props and the value of the Depth header

Properties are in the form <D:prop><prop1/><prop2/></D:prop>
If something unexpected happens during the xml parsing, ERR_NODATA
will be returned.

RFC-1518 requires that the xml must be validated and an error MUST
be returned to the client if the xml is not valid. This is not the case
in this funcion. It will accept many forms of invalid xml.

The original string post_param->data will be modified.
*/
static inline int get_props(connection_t* connection_prop,string_t* post_param,u_dav_details *props) {

    {
        //Determining if it is deep or not
        //props.deep=false; commented because redoundant
        char a[4]; //Buffer for field's value
        //Gets the value of content-length header
        bool r=get_param_value(connection_prop->http_param,"Depth", a,sizeof(a),strlen("Depth"));

        if (r) {
            props->dav_details.deep=(a[0]=='1');
        }
    }


    char *sprops[MAXPROPCOUNT];   //List of pointers to properties

    if (post_param->len==0) {//No specific prop request, sending everything
        props->dav_details.getetag=true;
        props->dav_details.getcontentlength=true;
        props->dav_details.resourcetype=true;
        props->dav_details.getlastmodified=true;
        // props->getcontenttype=false; Commented because redoundant

        return 0;
    }

//Locates the starting prop tag
    char*data=strstr(post_param->data,"<D:prop ");
    if (data==NULL)
        data=strstr(post_param->data,"<D:prop>");
    if (data==NULL)
        data=strstr(post_param->data,"<prop ");
    if (data==NULL)
        data=strstr(post_param->data,"<prop>");

    if (data==NULL) {
        return ERR_NODATA;
    }
    data+=6; //Eliminates the 1st useless tag

    {
        //Locates the ending prop tag
        char*end=strstr(data,"</D:prop>");
        if (end==NULL)
            end=strstr(data,"</prop>");

        if (end==NULL) {
            return ERR_NODATA;
        }
        end[0]=0;
    }
    int i;
    char *temp, *p_temp;
    for (i=0; (sprops[i]=strstr(data,"<"))!=NULL; i++,data=temp+1) {
        if (i==MAXPROPCOUNT-1) {//Reached limit
            sprops[i]=NULL;
            break;
        }
        sprops[i]+=1; //Removes the < stuff

        //Removes the />
        temp=strstr(sprops[i],"/>");
        if (temp==NULL) return ERR_NODATA;
        temp[0]=0;

        //Removing if there are parameters to the node
        p_temp=strstr(sprops[i]," ");
        if (p_temp!=NULL) {
            p_temp[0]=0;
        }
    }


    for (i=0; sprops[i]!=NULL; i++) {
        if (strstr(sprops[i],"getetag")!=NULL) {
            props->dav_details.getetag=true;
        } else if (strstr(sprops[i],"getcontentlength")!=NULL) {
            props->dav_details.getcontentlength=true;
        } else if (strstr(sprops[i],"resourcetype")!=NULL) {
            props->dav_details.resourcetype=true;
        } else if (strstr(sprops[i],"getlastmodified")!=NULL) { //Sends Date
            props->dav_details.getlastmodified=true;
#ifdef SEND_MIMETYPES
        } else if(strstr(sprops[i],"getcontenttype")!=NULL) { //Sends MIME type
            props->dav_details.getcontenttype=true;
#endif
        }
    }
    return 0;
}

/**
This function sends a xml property to the client.
It can be called only by funcions aware of this xml, because it sends only partial xml.

If the file can't be opened in readonly mode, this function does nothing.
*/
static inline int printprops(connection_t *connection_prop,u_dav_details props,char* file,char*filename,bool parent) {
    int sock=connection_prop->sock;
    struct stat stat_s;
    char escaped_filename[URI_LEN];
    char escaped_page[URI_LEN];

    int file_fd = open(file, O_RDONLY);
    if (file_fd == -1) return 0;
    fstat(file_fd, &stat_s);

    escape_uri(filename,escaped_filename,URI_LEN);
    escape_uri(connection_prop->page,escaped_page,URI_LEN);

    dprintf(sock, "<D:response>\n");

    {
        //Sends href of the resource
        if (parent) {
            dprintf(sock, "<D:href>%s</D:href>", escaped_filename);
        } else {
            dprintf(sock, "<D:href>%s%s</D:href>", escaped_page, escaped_filename);
        }
    }

    dprintf(sock, "<D:propstat><D:prop>");

    if (props.dav_details.getetag) {
        dprintf(sock, "<D:getetag>%ld</D:getetag>\n", stat_s.st_mtime);
    }

    if (props.dav_details.getcontentlength) {
        dprintf(sock, "<D:getcontentlength>%ld</D:getcontentlength>\n", stat_s.st_size);
    }

    if (props.dav_details.resourcetype) {//Directory or normal file
        const char* type;
        if (S_ISDIR(stat_s.st_mode)) {
            type = "<D:collection/>";
        } else {
            type = " ";
        }

        dprintf(sock, "<D:resourcetype>%s</D:resourcetype>\n", type);
    }

    if (props.dav_details.getlastmodified) { //Sends Date
        struct tm ts;
        localtime_r(&stat_s.st_mtime,&ts);
        char prop_buffer[URI_LEN];
        strftime(prop_buffer, URI_LEN, "%a, %d %b %Y %H:%M:%S GMT", &ts);
        dprintf(sock, "<D:getlastmodified>%s</D:getlastmodified>\n", prop_buffer);
    }

#ifdef SEND_MIMETYPES
    if(props.dav_details.getcontenttype) { //Sends MIME type
        thread_prop_t *thread_prop = pthread_getspecific(thread_key);

        const char* t = mime_get_fd(thread_prop->mime_token, file_fd, &stat_s);
        dprintf(sock, "<D:getcontenttype>%s</D:getcontenttype>\n", t);
    }
#endif

    dprintf(sock,
        "</D:prop><D:status>HTTP/1.1 200 OK</D:status></D:propstat>"
        "</D:response>");

    close(file_fd);
    return 0;
}

/**
This function serves a PROPFIND request.

Can serve both depth and non-depth requests. This funcion works only if
authentication is enabled.
*/
int propfind(connection_t* connection_prop,string_t *post_param) {
    //Forbids the method if no authentication is in use
    if (weborf_conf.authsock==NULL) {
        return ERR_FORBIDDEN;
    }

    int sock=connection_prop->sock;
    u_dav_details props= {0};
    props.dav_details.type=1; //I need to avoid the struct to be fully 0 in each case
    int swap_fd=-1; //swap file descriptor
    const bool has_cache=cache_is_enabled();

    {
        //This redirects directory without ending / to directory with the ending /
        int stat_r=stat(connection_prop->strfile, &connection_prop->strfile_stat);

        if (stat_r!=0) {
            return ERR_FILENOTFOUND;
        }

        if (S_ISDIR(connection_prop->strfile_stat.st_mode) && !endsWith(connection_prop->strfile,"/",connection_prop->strfile_len,1)) {//Putting the ending / and redirect
            char head[URI_LEN+12];//12 is the size for the location header
            snprintf(head,URI_LEN+12,"Location: %s/\r\n",connection_prop->page);
            send_http_header(301,0,head,true,-1,connection_prop);
            return 0;
        }
    } // End redirection

    int retval=get_props(connection_prop,post_param,&props);//splitting props
    if (retval!=0) {
        return retval;
    }

    //Sets keep alive to false (have no clue about how big is the generated xml) and sends a multistatus header code
    connection_prop->keep_alive=false;
    send_http_header(207,0,"Content-Type: text/xml; charset=\"utf-8\"\r\n",false,-1,connection_prop);

    //Check if exists in cache
    if (has_cache) {
        if ((swap_fd=cache_get_item_fd(props.int_version,connection_prop))!=-1) {
            //Sends the item stored in the cache
            struct stat sb;
            fstat(swap_fd,&sb);

            fd_copy(swap_fd,sock, sb.st_size);

            close(swap_fd);
            return 0;
        } else { //Prepares for storing the item in cache, will be sent afterwards
            //Get file descriptor of cache file
            int cache_fd=cache_get_item_fd_wr(props.int_version,connection_prop);

            //If we obtained that descriptor, replace socket with it
            if (cache_fd!=-1) {
                swap_fd=sock; //Stores the real socket
                connection_prop->sock=sock=cache_fd;
            } else
                swap_fd=-1;
        }

    }

    //Sends header of xml response
    dprintf(sock,
            "<?xml version=\"1.0\" encoding=\"utf-8\" ?>"
            "<D:multistatus xmlns:D=\"DAV:\">");

    //sends props about the requested file
    printprops(connection_prop, props, connection_prop->strfile, connection_prop->page, true);

    if (props.dav_details.deep) {//Send children files
        DIR *dp = opendir(connection_prop->strfile); //Open dir
        char file[URI_LEN];
        struct dirent *entry;
        int return_code;

        if (dp == NULL) {//Error, unable to send because header was already sent
            close(sock);
            return 0;
        }

        while ((entry=readdir(dp)) != NULL) { //Cycles trough dir's elements
#ifdef HIDE_HIDDEN_FILES
            if (entry->d_name[0]=='.') //doesn't list hidden files
                continue;
#else
            //Avoids dir . and .. but not all hidden files
            if (entry->d_name[0]=='.' && (entry->d_name[1]==0 || (entry->d_name[1]=='.' && entry->d_name[2]==0)))
                continue;
#endif
            snprintf(file, URI_LEN, "%s%s", connection_prop->strfile, entry->d_name);

            //Sends details about a file
            printprops(connection_prop, props, file, entry->d_name, false);
        }

        closedir(dp);
    }
    //ends multistatus
    dprintf(sock, "</D:multistatus>");

    /*
     * If we were able to get a file descriptor for the cache file, and cache is enabled,
     * at this point the XMP has been saved into the cache but not sent to the client,
     * so now we read from cache and send to the client
     */
    if (has_cache && swap_fd!=-1) {
        int cache_fd=sock;
        connection_prop->sock=sock=swap_fd; //Restore sock to its value

        off_t prev_pos=lseek(cache_fd,0,SEEK_CUR); //Get size of the file
        lseek(cache_fd,0,SEEK_SET);          //Set cursor to the beginning
        fd_copy(cache_fd,sock,prev_pos);     //Send the file

        close(cache_fd);
    }

    return 0;
}

/**
This funcion should be named mkdir. But standards writers are weird people.
*/
int mkcol(connection_t* connection_prop) {
    if (weborf_conf.authsock==NULL) {
        return ERR_FORBIDDEN;
    }

    int res=mkdir(connection_prop->strfile,S_IRWXU | S_IRWXG | S_IRWXO);

    if (res==0) {//Directory created
        return OK_CREATED;
    }

    //Error
    switch (errno) {
    case EACCES:
    case EFAULT:
    case ELOOP:
    case ENAMETOOLONG:
        return ERR_FORBIDDEN;
    case ENOMEM:
        return ERR_SERVICE_UNAVAILABLE;
    case ENOENT:
        return ERR_CONFLICT;
    case EEXIST:
    case ENOTDIR:
        return ERR_NOT_ALLOWED;
    case ENOSPC:
    case EROFS:
    case EPERM:
        return ERR_INSUFFICIENT_STORAGE;
    }

    return ERR_SERVICE_UNAVAILABLE; //Make gcc happy
}

/**
Webdav method copy.
*/
int copy_move(connection_t* connection_prop) {
    struct stat f_prop; //File's property
    bool check_exists=false;
    int retval=0;
    bool exists;

    char* host=malloc(3*PATH_LEN+12);
    if (host==NULL) {
        return ERR_NOMEM;
    }
    char* dest=host+PATH_LEN;
    char* overwrite=dest+PATH_LEN;
    char* destination=overwrite+2;

    //If the file has the same date, there is no need of sending it again
    bool host_b=get_param_value(connection_prop->http_param,"Host",host,PATH_LEN,strlen("Host"));
    bool dest_b=get_param_value(connection_prop->http_param,"Destination",dest,PATH_LEN,strlen("Destination"));
    bool overwrite_b=get_param_value(connection_prop->http_param,"Overwrite",overwrite,PATH_LEN,strlen("Overwrite"));

    if (host_b && dest_b == false) { //Some important header is missing
        retval=ERR_NOTHTTP;
        goto escape;
    }

    /*Sets if there is overwrite or not.
    ovewrite header is a boolean where F is false.
    */
    if (overwrite_b) {
        check_exists=(overwrite[0]!='F');
    }

    dest=strstr(dest,host);
    if (dest==NULL) {//Something is wrong here
        retval=ERR_NOTHTTP;
        goto escape;
    }
    dest+=strlen(host);

    //Local path for destination file
    snprintf(destination,PATH_LEN,"%s%s",connection_prop->basedir,dest);

    if (strcmp(connection_prop->strfile,destination)==0) {//same
        retval=ERR_FORBIDDEN;
        goto escape;
    }
    exists=file_exists(destination);

    //Checks if the file already exists
    if (check_exists && exists) {
        retval=ERR_PRECONDITION_FAILED;
        goto escape;
    }

    stat(connection_prop->strfile, &f_prop);
    if (S_ISDIR(f_prop.st_mode)) { //Directory
        if (connection_prop->method_id==COPY) {
            retval=dir_copy(connection_prop->strfile,destination);
        } else {//Move
            retval=dir_move(connection_prop->strfile,destination);
        }
    } else { //Normal file
        if (connection_prop->method_id==COPY) {
            retval=file_copy(connection_prop->strfile,destination);
        } else {//Move
            retval=file_move(connection_prop->strfile,destination);
        }
    }

    if (retval==0) {
        retval=exists?OK_NOCONTENT:OK_CREATED;
    }
escape:
    free(host);
    return retval;
}

#endif
