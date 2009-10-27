

#ifndef WEBORF_WEBDAV_H
#define WEBORF_WEBDAV_H

#include <string.h>
#include <stdbool.h>

#include "instance.h"


int propfind(int sock,connection_t* connection_prop,string_t *post_param);

#endif