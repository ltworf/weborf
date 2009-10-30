

#ifndef WEBORF_WEBDAV_H
#define WEBORF_WEBDAV_H

#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>


#include "instance.h"


int propfind(int sock,connection_t* connection_prop,string_t *post_param);

#endif