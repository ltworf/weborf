
#include "webdav.h"


int propfind(int sock,connection_t* connection_prop,string_t *post_param) {
    //TODO must not work without authentication
    write(1,post_param->data,post_param->len);
    
    char*data=strstr(post_param->data,"<D:prop>");
    if (data==NULL)
        return ERR_NODATA;
    
    {
        char*end=strstr(data,"</D:prop>");
        if (end==NULL)
            return ERR_NODATA;
        end[0]=0;
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

{

}

printf("%s\n",data);

return 0;
}