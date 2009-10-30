

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

int printprops(int sock,connection_t* connection_prop,char*props[],char* file) {
    write(sock,"<D:response>",12);

    int i,p_len;
    struct stat stat_s;
    stat(file, &stat_s);
    char buffer[URI_LEN];

    /*           struct stat {
               dev_t     st_dev;     /* ID of device containing file
               ino_t     st_ino;     /* inode number
               mode_t    st_mode;    /* protection
               nlink_t   st_nlink;   /* number of hard links *
               uid_t     st_uid;     /* user ID of owner *
               gid_t     st_gid;     /* group ID of owner *
               dev_t     st_rdev;    /* device ID (if special file) *
               off_t     st_size;    /* total size, in bytes *
               blksize_t st_blksize; /* blocksize for file system I/O *
               blkcnt_t  st_blocks;  /* number of 512B blocks allocated *
               time_t    st_atime;   /* time of last access *
               time_t    st_mtime;   /* time of last modification *
               time_t    st_ctime;   /* time of last status change *
           };
    */

    {//Sends href of the resource
        char host[URI_LEN];

        if (!get_param_value(connection_prop->http_param,"Host", host, URI_LEN,4)) {
            return ERR_NOTHTTP; //Invalid request
        }
        write(sock,"<D:href>http://",15);//TODO protocol will have to be https when it will be implemented
        write(sock,host,strlen(host));
        write(sock,connection_prop->page,connection_prop->page_len);
        write(sock,"</D:href>",9);
    }

    write(sock,"<D:propstat><D:prop>",20);
    {//Writing properties

        char prop_buffer[URI_LEN];
        for (i=0; props[i]!=NULL; i++) {
            //printf("%s\n",props[i]);

            prop_buffer[0]=0;

            p_len-=2;
            if (strncmp(props[i],"D:getetag",p_len)==0) {
                p_len=snprintf(prop_buffer,URI_LEN,"%d",stat_s.st_mtime);
            } else if (strncmp(props[i],"D:getcontentlength",p_len)==0) {
                p_len=snprintf(prop_buffer,URI_LEN,"%d",stat_s.st_size);
            } else if (strncmp(props[i],"D:resourcetype",p_len)==0) {
                if (S_ISDIR(stat_s.st_mode)) {
                    snprintf(prop_buffer,URI_LEN,"<D:collection/>");
                } else {
                    prop_buffer[0]=' ';
                    prop_buffer[1]=0;
                }

            }

            /*
            if (strncmp(props[i],"D:getetag",p_len)==0) {
                p_len=snprintf(prop_buffer,URI_LEN,"%d",stat_s.st_mtime);
            }
            */

            /*
            D:creationdate
            D:displayname
            D:source
            D:getcontentlanguage
            D:getcontenttype
            D:executable
            D:getlastmodified
            D:supportedlock
            D:lockdiscovery
            D:resourcetype
            */
            if (prop_buffer[0]!=0) {
                p_len=snprintf(buffer,URI_LEN,"<%s>%s</%s>\n",props[i],prop_buffer,props[i]);
                write (sock,buffer,p_len);
                props[i][0]=' ';
            }

        }
    }

    /*       <R:bigbox>
                         <R:BoxType>Box type A</R:BoxType>
                    </R:bigbox>
                    <R:author>
                         <R:Name>Hadrian</R:Name>
                    </R:author>
                    <D:creationdate>
                         1997-12-01T17:42:21-08:00
                    </D:creationdate>
                    <D:displayname>
                         Example collection
                    </D:displayname>


    */
    write(sock,"</D:prop><D:status>HTTP/1.1 200 OK</D:status></D:propstat>",58);

    //TODO write invalid props
    write(sock,"<D:propstat><prop>",18);
    for (i=0; props[i]!=NULL; i++) {
        if (props[i][0]!=' ') {
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


    sock=1; //TODO Remove this!!!

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

    printprops(sock,connection_prop,props,connection_prop->strfile);

    write(sock,"</D:multistatus>",16);


    return 0;
}

#endif