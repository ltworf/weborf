

#include "webdav.h"

#ifdef WEBDAV

extern char* authbin;

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
    char*data=strstr(post_param->data,"<D:prop ");
    if (data==NULL)
        data=strstr(post_param->data,"<D:prop>");

    if (data==NULL)
        return ERR_NODATA;
    data+=8; //Eliminates the 1st useless tag

    {
        char*end=strstr(data,"</D:prop>");
        if (end==NULL)
            return ERR_NODATA;
        end[0]=0;
    }

    int i;
    char* temp;
    for (i=0; (props[i]=strstr(data,"<"))!=NULL; i++,data=temp+1) {
        if (i==MAXPROPCOUNT-1) {//Reached limit
            props[i]=NULL;
            break;
        }
        props[i]+=1; //Removes the <D: stuff
        temp=strstr(props[i],"/>");
        temp[0]=0;
    }
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
    

    stat(file, &stat_s);
    write(sock,"<D:response>\n",13);
    
    /*           struct stat {
               dev_t     st_dev;     / ID of device containing file
               ino_t     st_ino;     / inode number
               mode_t    st_mode;    / protection
               nlink_t   st_nlink;   / number of hard links *
               uid_t     st_uid;     / user ID of owner *
               gid_t     st_gid;     / group ID of owner *
               dev_t     st_rdev;    / device ID (if special file) *
               off_t     st_size;    / total size, in bytes *
               blksize_t st_blksize; / blocksize for file system I/O *
               blkcnt_t  st_blocks;  / number of 512B blocks allocated *
               time_t    st_atime;   / time of last access *
               time_t    st_mtime;   / time of last modification *
               time_t    st_ctime;   / time of last status change *
           };
    */

    {//Sends href of the resource        
        if (parent){
            p_len=snprintf(buffer,URI_LEN,"<D:href>%s</D:href>",filename);
        } else {
            p_len=snprintf(buffer,URI_LEN,"<D:href>%s%s</D:href>",connection_prop->page,filename);
        }
        
        write (sock,buffer,p_len);
    }

    write(sock,"<D:propstat><D:prop>",20);
    {//Writing properties
        char prop_buffer[URI_LEN];
        for (i=0; props[i]!=NULL; i++) {
            props_invalid[i]=false;

            prop_buffer[0]=0;
            p_len-=2;

            if (strncmp(props[i],"D:getetag",9)==0) {
                p_len=snprintf(prop_buffer,URI_LEN,"%d",stat_s.st_mtime);
                goto end_comp;
            } else if (strncmp(props[i],"D:getcontentlength",18)==0) {
                p_len=snprintf(prop_buffer,URI_LEN,"%lld",(long long)stat_s.st_size);
                goto end_comp;
            } else if (strncmp(props[i],"D:resourcetype",14)==0) {
                if (S_ISDIR(stat_s.st_mode)) {
                    snprintf(prop_buffer,URI_LEN,"<D:collection/>");
                } else {
                    prop_buffer[0]=' ';
                    prop_buffer[1]=0;
                }
                goto end_comp;
            } else if (strncmp(props[i],"D:getlastmodified",17)==0) { //Sends Date
                struct tm ts;
                localtime_r((time_t)&stat_s.st_mtime,&ts);
                strftime(prop_buffer,URI_LEN, "%a, %d %b %Y %H:%M:%S GMT", &ts);
                goto end_comp;
            } else {
                props_invalid[i]=true;
            }
end_comp: //goto pointing here are optimization. It would work also without
            /*
            if (strncmp(props[i],"D:getetag",p_len)==0) {
                p_len=snprintf(prop_buffer,URI_LEN,"%d",stat_s.st_mtime);
            }
            */

            /*
            D:displayname
            D:source
            D:getcontentlanguage
            D:getcontenttype
            D:executable
            D:supportedlock
            D:lockdiscovery
            D:resourcetype
            */
            if (prop_buffer[0]!=0) {
                p_len=snprintf(buffer,URI_LEN,"<%s>%s</%s>\n",props[i],prop_buffer,props[i]);
                write (sock,buffer,p_len);
            }

        }
    }
    write(sock,"</D:prop><D:status>HTTP/1.1 200 OK</D:status></D:propstat>",58);

    write(sock,"<D:propstat><prop>",18);
    for (i=0; props[i]!=NULL; i++) {
        if (props_invalid[i]==true) {
            p_len=snprintf(buffer,URI_LEN,"<%s/>\n",props[i]);
            write (sock,buffer,p_len);
        }
    }
    write(sock,"</prop><D:status>HTTP/1.1 404 Not Found</D:status></D:propstat>",63);
    write(sock,"</D:response>",13);

    return 0;
}

/**
This function serves a PROPFIND request.

Can serve both depth and non-depth requests. This funcion works only if
authentication is enabled.


*/
int propfind(int sock,connection_t* connection_prop,string_t *post_param) {
    if (authbin==NULL) {
        return ERR_FORBIDDEN;
    }
    
    {
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
    }

    char *props[MAXPROPCOUNT];   //List of pointers to properties
    bool deep=false;

    
    { //Determining if it is deep or not
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
    
    //sock=1;
    
    //sends props about the requested file
    printprops(sock,connection_prop,props,connection_prop->strfile,connection_prop->page,true);
    if (deep) {//Send children files
        
        struct dirent *ep; //File's property
        DIR *dp = opendir(connection_prop->strfile); //Open dir
        char file[URI_LEN];

        if (dp == NULL) {//Error, unable to send because header was already sent
            close(sock);
            return 0;
        }
        while ((ep = readdir(dp))) { //Cycles trough dir's elements

#ifdef HIDE_HIDDEN_FILES
            if (ep->d_name[0]=='.') //doesn't list hidden files
                continue;
#else
            //Avoids dir . and .. but not all hidden files
            if (ep->d_name[0]=='.' && (ep->d_name[1]==0 || (ep->d_name[1]=='.' && ep->d_name[2]==0)))
                continue;
#endif

            snprintf(file,URI_LEN,"%s%s", connection_prop->strfile, ep->d_name);
            
            //Sends details about a file
            printprops(sock,connection_prop,props,file,ep->d_name,false);
        }
        
        closedir(dp);
    }

    //ends multistatus
    write(sock,"</D:multistatus>",16);

    return 0;
}

#endif