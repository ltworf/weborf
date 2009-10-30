

#include "webdav.h"

#ifdef WEBDAV

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

int printprops(int sock,connection_t* connection_prop,char*props[],char* file,char*filename) {
    write(sock,"<D:response>\n\n",14);

    int i,p_len;
    struct stat stat_s;

    stat(file, &stat_s);

    char buffer[URI_LEN];
    bool props_invalid[MAXPROPCOUNT]; //Used to keep trace of invalid props

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
        char host[URI_LEN];

        if (!get_param_value(connection_prop->http_param,"Host", host, URI_LEN,4)) {
            return ERR_NOTHTTP; //Invalid request
        }

        //TODO protocol will have to be https when it will be implemented
        if (S_ISDIR(stat_s.st_mode)) {//If is a directory, needs the ending /
            p_len=snprintf(buffer,URI_LEN,"<D:href>http://%s/%s/</D:href>",host,filename);
        } else {
            p_len=snprintf(buffer,URI_LEN,"<D:href>http://%s/%s</D:href>",host,filename);
        }
        write (sock,buffer,p_len);
    }

    write(sock,"<D:propstat><D:prop>",20);
    {//Writing properties
        char prop_buffer[URI_LEN];
        for (i=0; props[i]!=NULL; i++) {
            //printf("%s\n",props[i]);
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
end_comp:
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
                //printf("%s\t:\t%s\n",props[i],prop_buffer);

                p_len=snprintf(buffer,URI_LEN,"<%s>%s</%s>\n",props[i],prop_buffer,props[i]);
                write (sock,buffer,p_len);
                //props[i][0]=' ';
            }

        }
    }
    write(sock,"</D:prop><D:status>HTTP/1.1 200 OK</D:status></D:propstat>",58);

    //TODO write invalid props
    write(sock,"<D:propstat><prop>",18);
    for (i=0; props[i]!=NULL; i++) {
        if (props_invalid[i]==true) {
            p_len=snprintf(buffer,URI_LEN,"<%s/>\n",props[i]);
            write (sock,buffer,p_len);
        }
    }
    write(sock,"</prop><D:status>HTTP/1.1 404 Not Found</D:status></D:propstat>",63);

    write(sock,"</D:response>",13);

    return 0; //Make gcc happy
}

int propfind(int sock,connection_t* connection_prop,string_t *post_param) {
    //TODO must not work without authentication
    //TODO if it is dir redirect to ending / version

    char *props[MAXPROPCOUNT];   //List of pointers to index files
    int retval;
    bool deep=false;

    printf("%s %s\n\n",connection_prop->page,connection_prop->http_param);
    //Determining if it is deep or not
    {
        char a[4]; //Buffer for field's value
        //Gets the value of content-length header
        bool r=get_param_value(connection_prop->http_param,"Depth", a,4,5);//14 is content-lenght's len

        if (r==true) {
            deep=a[0]=='1';
        }
    }

    retval=get_props(post_param,props);//splitting props
    if (retval!=0) {
        return retval;
    }

    connection_prop->keep_alive=false;
    send_http_header_full(sock,207,0,"Connection: close\r\nContent-Type: text/xml; charset=\"utf-8\"\r\n",false,-1,connection_prop);

    write(sock,"<?xml version=\"1.0\" encoding=\"utf-8\" ?>",39);
    write(sock,"<D:multistatus xmlns:D=\"DAV:\">",30);

    printprops(sock,connection_prop,props,connection_prop->strfile,connection_prop->page);
    //TODO check if it really is a directory
    if (deep) {//Send files within the dir
        //sock=1;
        struct dirent *ep; //File's property
        DIR *dp = opendir(connection_prop->strfile); //Open dir
        char file[URI_LEN];

        //TODO if (dp == NULL) FAIL
        while ((ep = readdir(dp))) { //Cycles trough dir's elements
#ifdef HIDE_HIDDEN_FILES
            if (ep->d_name[0]=='.')
                continue;
#else

            //Avoids dir . and ..
            if (ep->d_name[0]=='.' && (ep->d_name[1]==0 || (ep->d_name[1]=='.' && ep->d_name[2]==0)))
                continue;
#endif

            snprintf(file,URI_LEN,"%s%s", connection_prop->strfile, ep->d_name);
            printprops(sock,connection_prop,props,file,ep->d_name);
        }
    }

    write(sock,"</D:multistatus>",16);

    return 0;
}

#endif