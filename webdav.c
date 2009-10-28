

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
    for (i=0;(props[i]=strstr(data,"<"))!=NULL;i++,data=temp+1) {
        if (i==MAXPROPCOUNT-1){//Reached limit
            props[i]=NULL;
            break;
        }           
        props[i]+=1; //Removes the <D: stuff
        temp=strstr(props[i],"/>");
        temp[0]=0;
        }
    return 0;
}

int printprops(int sock,connection_t* connection_prop,char*props[],char* file){
    write(sock,"<D:response>",12);
    
    
    
    /*<D:href>http://www.foo.bar/container/</D:href>*/
    
    
    write(sock,"<D:propstat>",12);
    /*       <D:prop xmlns:R="http://www.foo.bar/boxschema/">
                    <R:bigbox>
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
                    <D:resourcetype><D:collection/></D:resourcetype>
               </D:prop>
               <D:status>HTTP/1.1 200 OK</D:status>
*/
    write(sock,"</D:propstat>",13);
    
    //TODO write invalid props
    write(sock,"</D:response>",13);
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
    
    int i;
    for (i=0;props[i]!=NULL;i++){
    printf("%s\n",props[i]);
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