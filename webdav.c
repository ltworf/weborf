
#include "webdav.h"

int get_props(string_t* post_param,char * props[]) {
    char*data=strstr(post_param->data,"<D:prop>");
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
    for (i=0;(props[i]=strstr(data,"<D:"))!=NULL;i++,data=temp+1) {
        if (i==MAXPROPCOUNT-1){//Reached limit
            props[i]=NULL;
            break;
        }           
        props[i]+=3; //Removes the <D: stuff
        temp=strstr(props[i],"/>");
        temp[0]=0;
        }
    return 0;
}

int propfind(int sock,connection_t* connection_prop,string_t *post_param) {
    //TODO must not work without authentication
    
    char *props[MAXPROPCOUNT];   //List of pointers to index files
    int retval;
    
    retval=get_props(post_param,props);//splitting props
    if (retval!=0) {
        return retval;
    }
    
    int i;
    for (i=0;props[i]!=NULL;i++){
    printf("%s\n",props[i]);
    }
       
/*
<D:propfind xmlns:D="DAV:">
<D:prop>
    <D:creationdate/>
    <D:getcontentlength/>
    <D:displayname/>
    <D:source/>
    <D:getcontentlanguage/>
    <D:getcontenttype/>
    <D:executable/>
    <D:getlastmodified/>
    <D:getetag/>
    <D:supportedlock/>
    <D:lockdiscovery/>
    <D:resourcetype/>
</D:prop>
</D:propfind>
*/

return 0;
}