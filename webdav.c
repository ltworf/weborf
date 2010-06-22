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

#include "types.h"
#include "options.h"

#ifdef WEBDAV

#include "webdav.h"

extern char* authsock;
extern char* basedir;
extern bool virtual_host;

/**
This function will split the required props into a char* array.

Properties are in the form <D:prop><prop1/><prop2/></D:prop>
If something unexpected happens during the xml parsing, ERR_NODATA
will be returned.

RFC-1518 requires that the xml must be validated and an error MUST
be returned to the client if the xml is not valid. This is not the case
in this funcion. It will accept many forms of invalid xml.

The original string post_param->data will be modified.
*/
int get_props(string_t* post_param,char * props[]) {
    if (post_param->len==0) {//No specific prop request, sending everything
        goto default_prop;
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
    for (i=0; (props[i]=strstr(data,"<"))!=NULL; i++,data=temp+1) {
        if (i==MAXPROPCOUNT-1) {//Reached limit
            props[i]=NULL;
            break;
        }
        props[i]+=1; //Removes the < stuff

        //Removes the />
        temp=strstr(props[i],"/>");
        if (temp==NULL)
            return ERR_NODATA;
        
        temp[0]=0;

        //Removing if there are parameters to the node
        p_temp=strstr(props[i]," ");
        if (p_temp!=NULL) {
            p_temp[0]=0;
        }
    }
    return 0;
default_prop:
    //Sets the array to some default props
    props[0]="getetag";
    props[1]="getcontentlength";
    props[2]="resourcetype";
    props[3]="getlastmodified";
    props[4]=NULL;

    return 0;
}

/**
This function sends a xml property to the client.
It can be called only by funcions aware of this xml, because it sends only partial xml.
*/
int printprops(int sock,connection_t* connection_prop,char*props[],char* file,char*filename,bool parent) {
    int i,p_len;
    struct stat stat_s;
    char buffer[URI_LEN];
    bool props_invalid[MAXPROPCOUNT]; //Used to keep trace of invalid props
    bool invalid_props=false; //Used to avoid sending the invalid props if there isn't any

    stat(file, &stat_s);
    write(sock,"<D:response>\n",13);

    {
        //Sends href of the resource
        if (parent) {
            p_len=snprintf(buffer,URI_LEN,"<D:href>%s</D:href>",filename);
        } else {
            p_len=snprintf(buffer,URI_LEN,"<D:href>%s%s</D:href>",connection_prop->page,filename);
        }

        write (sock,buffer,p_len);
    }

    write(sock,"<D:propstat><D:prop>",20);
    {
        //Writing properties
        char prop_buffer[URI_LEN];
        for (i=0; props[i]!=NULL; i++) {
            props_invalid[i]=false;

            prop_buffer[0]=0;
            p_len-=2;

            if (strstr(props[i],"getetag")!=NULL) {
                //The casting might be wrong on some architectures
                p_len=snprintf(prop_buffer,URI_LEN,"%d",(unsigned int)stat_s.st_mtime);
                goto end_comp;
            } else if (strstr(props[i],"getcontentlength")!=NULL) {
                p_len=snprintf(prop_buffer,URI_LEN,"%lld",(long long)stat_s.st_size);
                goto end_comp;
            } else if (strstr(props[i],"resourcetype")!=NULL) {
                if (S_ISDIR(stat_s.st_mode)) {
                    snprintf(prop_buffer,URI_LEN,"<D:collection/>");
                } else {
                    prop_buffer[0]=' ';
                    prop_buffer[1]=0;
                }
                goto end_comp;
            } else if (strstr(props[i],"getlastmodified")!=NULL) { //Sends Date
                struct tm ts;
                localtime_r(&stat_s.st_mtime,&ts);
                strftime(prop_buffer,URI_LEN, "%a, %d %b %Y %H:%M:%S GMT", &ts);
                goto end_comp;
            } else {
                invalid_props=true;
                props_invalid[i]=true;
            }
end_comp: //goto pointing here are optimization. It would work also without
            /*
            if (strncmp(props[i],"D:getetag",p_len)==0) {
                p_len=snprintf(prop_buffer,URI_LEN,"%d",stat_s.st_mtime);
            }
            */

            if (prop_buffer[0]!=0) {
                p_len=snprintf(buffer,URI_LEN,"<%s>%s</%s>\n",props[i],prop_buffer,props[i]);
                write (sock,buffer,p_len);
            }

        }
    }
    write(sock,"</D:prop><D:status>HTTP/1.1 200 OK</D:status></D:propstat>",58);

    if (invalid_props) {
        write(sock,"<D:propstat><prop>",18);
        for (i=0; props[i]!=NULL; i++) {
            if (props_invalid[i]==true) {
                p_len=snprintf(buffer,URI_LEN,"<%s/>\n",props[i]);
                write (sock,buffer,p_len);
            }
        }
        write(sock,"</prop><D:status>HTTP/1.1 404 Not Found</D:status></D:propstat>",63);
    }
    write(sock,"</D:response>",13);

    return 0;
}

/**
This function serves a PROPFIND request.

Can serve both depth and non-depth requests. This funcion works only if
authentication is enabled.
*/
int propfind(int sock,connection_t* connection_prop,string_t *post_param) {
    if (authsock==NULL) {
        return ERR_FORBIDDEN;
    }

    {
        //This redirects directory without ending / to directory with the ending /
        struct stat stat_s;
        int stat_r=stat(connection_prop->strfile, &stat_s);

        if (stat_r!=0) {
            return ERR_FILENOTFOUND;
        }

        if (S_ISDIR(stat_s.st_mode) && !endsWith(connection_prop->strfile,"/",connection_prop->strfile_len,1)) {//Putting the ending / and redirect
            char head[URI_LEN+12];//12 is the size for the location header
            snprintf(head,URI_LEN+12,"Location: %s/\r\n",connection_prop->page);
            send_http_header_full(sock,301,0,head,true,-1,connection_prop);
            return 0;
        }
    } // End redirection

    char *props[MAXPROPCOUNT];   //List of pointers to properties
    bool deep=false;


    {
        //Determining if it is deep or not
        char a[4]; //Buffer for field's value
        //Gets the value of content-length header
        bool r=get_param_value(connection_prop->http_param,"Depth", a,4,5);//14 is content-lenght's len

        if (r==true) {
            deep=a[0]=='1';
        }
    }

    int retval;
    retval=get_props(post_param,props);//splitting props
    if (retval!=0) {
        return retval;
    }

    //Sets keep alive to false (have no clue about how big is the generated xml) and sends a multistatus header code
    connection_prop->keep_alive=false;
    send_http_header_full(sock,207,0,"Connection: close\r\nContent-Type: text/xml; charset=\"utf-8\"\r\n",false,-1,connection_prop);

    //Sends header of xml response
    write(sock,"<?xml version=\"1.0\" encoding=\"utf-8\" ?>",39);
    write(sock,"<D:multistatus xmlns:D=\"DAV:\">",30);

    //sends props about the requested file
    printprops(sock,connection_prop,props,connection_prop->strfile,connection_prop->page,true);
    if (deep) {//Send children files
        DIR *dp = opendir(connection_prop->strfile); //Open dir
        char file[URI_LEN];
        struct dirent entry;
        struct dirent *result;
        int return_code;

        if (dp == NULL) {//Error, unable to send because header was already sent
            close(sock);
            return 0;
        }

        for (return_code=readdir_r(dp,&entry,&result); result!=NULL && return_code==0; return_code=readdir_r(dp,&entry,&result)) { //Cycles trough dir's elements
#ifdef HIDE_HIDDEN_FILES
            if (entry.d_name[0]=='.') //doesn't list hidden files
                continue;
#else
            //Avoids dir . and .. but not all hidden files
            if (entry.d_name[0]=='.' && (entry.d_name[1]==0 || (entry.d_name[1]=='.' && entry.d_name[2]==0)))
                continue;
#endif

            snprintf(file,URI_LEN,"%s%s", connection_prop->strfile, entry.d_name);

            //Sends details about a file
            printprops(sock,connection_prop,props,file,entry.d_name,false);
        }

        closedir(dp);
    }

    //ends multistatus
    write(sock,"</D:multistatus>",16);

    return 0;
}

/**
This funcion should be named mkdir. But standards writers are weird people.
*/
int mkcol(int sock,connection_t* connection_prop) {
    if (authsock==NULL) {
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
int copy_move(int sock,connection_t* connection_prop) {
    struct stat f_prop; //File's property
    bool deep=true;
    bool check_exists=false;
    int retval=0;
    char* real_basedir;
    bool exists;

    char* host=malloc(3*PATH_LEN+12);
    if (host==NULL) {
        return ERR_NOMEM;
    }
    char* dest=host+PATH_LEN;
    char* depth=dest+PATH_LEN;
    char* overwrite=depth+10;
    char* destination=overwrite+2;

    //If the file has the same date, there is no need of sending it again
    bool host_b=get_param_value(connection_prop->http_param,"Host",host,RBUFFER,4);
    bool dest_b=get_param_value(connection_prop->http_param,"Destination",dest,RBUFFER,11);
    bool depth_b=get_param_value(connection_prop->http_param,"Depth",depth,RBUFFER,5);
    bool overwrite_b=get_param_value(connection_prop->http_param,"Overwrite",overwrite,RBUFFER,9);

    if (host_b && dest_b == false) { //Some important header is missing
        retval=ERR_NOTHTTP;
        goto escape;
    }

    /*Sets if there is overwrite or not.
    ovewrite header is a boolean where F is false.
    */
    if (overwrite_b) {
        check_exists=overwrite[0]!='F';
    }

    //Set the depth of the copy (valid just in case of directory
    if (depth_b) {
        deep=depth[0]=='0'?false:true;
    }

    dest=strstr(dest,host);
    if (dest==NULL) {//Something is wrong here
        retval=ERR_NOTHTTP;
        goto escape;
    }
    dest+=strlen(host);

    if (virtual_host) { //Using virtual hosts
        real_basedir=get_basedir(connection_prop->http_param);
        if (real_basedir==NULL) real_basedir=basedir;
    } else {//No virtual Host
        real_basedir=basedir;
    }

    //Local path for destination file
    snprintf(destination,PATH_LEN,"%s%s",real_basedir,dest);

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
