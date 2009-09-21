/*
Weborf
Copyright (C) 2007  Salvo "LtWorf" Tomaselli

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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "utils.h"
#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include "options.h"
#include "mystring.h"

/**
This function reads the directory dir, putting inside the html string an html page with links to all the files within the directory.

Buffer for html must be allocated by the calling function.
bufsize is the size of the buffer allocated for html. To avoid buffer overflows.
*/
int list_dir(char *dir, char *html, unsigned int bufsize, bool parent) {
    //TODO replace strncat with something more efficient
    int maxsize = bufsize - 1;	//String's max size
    struct dirent *ep;		//File's property
    DIR *dp = opendir(dir);	//Open dir

    if (dp != NULL) {		//Open succesfull
        html = strncat(html, HTMLHEAD, maxsize);	//Puts html header within the string

        char buf[INBUFFER];	//Buffer to contain the element
        char path[INBUFFER];	//Buffer to contain element's absolute path

        //Specific header table)
        strncat(html,
                "<table><tr><td></td><td>Name</td><td>Size</td></tr>",
                maxsize);

        unsigned int counter = 0;
        while ((ep = readdir(dp))) {	//Cycles trough dir's elements
            counter++;		//Increases counter, to know if the row is odd or even
            sprintf(path, "%s/%s", dir, ep->d_name);

            struct stat f_prop;	//File's property
            stat(path, &f_prop);
            int f_mode = f_prop.st_mode;	//Get's file's mode

            if (S_ISREG(f_mode)) {	//Regular file

                //Table row for the file

                //Scaling the file's size
                char measure[3];
                unsigned int size = f_prop.st_size;
                if (size < 1024) {
                    snprintf(measure, 3, "B");
                } else if ((size = (size / 1024)) < 1024) {
                    snprintf(measure, 3, "KB");
                } else {
                    size = size / 1024;
                    snprintf(measure, 3, "MB");
                }

                char *color;	//Depending on row count chooses a background color
                if (counter % 2 == 0)
                    color = "white";
                else
                    color = "#EAEAEA";


                sprintf(buf,
                        "<tr style=\"background-color: %s;\"><td>f</td><td><a href=\"%s\">%s</a></td><td>%u%s</td></tr>\n",
                        color, ep->d_name, ep->d_name, size, measure);

            } else if (S_ISDIR(f_mode)) {	//Directory
                bool print = true;
                //Table row for the dir
                if (ep->d_name[0] == '.' && ep->d_name[1] == '.' && ep->d_name[2] == '\0') {	//Parent entry
                    //If showing parent entry is forbidden, doesn't show it
                    if (parent == false)
                        print = false;
                } else if (ep->d_name[0] == '.' && ep->d_name[1] == '\0') {	//Self entry
                    //doesn't show it
                    print = false;
                }

                if (print) {

                    sprintf(buf,
                            "<tr style=\"background-color: #DFDFDF;\"><td>d</td><td><a href=\"%s/\">%s/</a></td><td>-</td></tr>\n",
                            ep->d_name, ep->d_name);
                } else {
                    buf[0] = '\0';
                }

            }
            //Insert row into the html
            html = strncat(html, buf, maxsize);
        }

        //Table's footer
        strncat(html, "</table>", maxsize);

        closedir(dp);


    } else {			//Error opening the directory
        return -1;
    }

    //Adds html footer
    html = strncat(html, HTMLFOOT, maxsize);

    return 0;
}



/**
Returns true if the specified file exists
*/
bool file_exists(char *file) {
    int fp = open(file, O_RDONLY);
    if (fp >= 0) {		// exists
        close(fp);
    } else {			//doesn't exists
        return false;
    }
    return true;
}

/**
Returns file's mode
-1 in case of error
*/
int fileIsA(char *file) {

    struct stat buf;
    if (stat(file, &buf) < 0) {
        return -1;
    }
    return (buf.st_mode);
}

/**
Finds the first 0 char and returns a pointer to the next char
*/
char *findNext(char *str) {
    return (char *) (str + strlen(str) + 2);
}

/**
Prints version information
*/
void version() {
    printf("Weborf %s\n", VERSION);
    printf("Copyright (C) 2007 Salvo 'LtWorf' Tomaselli.\n");
    printf
    ("This is free software.  You may redistribute copies of it under the terms of\n");
    printf
    ("the GNU General Public License <http://www.gnu.org/licenses/gpl.html>.\n");
    printf("There is NO WARRANTY, to the extent permitted by law.\n\n");

    printf("Written by Salvo 'LtWorf' Tomaselli and Salvo Rinaldi.\n");
    printf("Synchronized queue by Prof. Giuseppe Pappalardo.\n\n\n");
    printf("https://galileo.dmi.unict.it/wiki/weborf/\n");
    exit(0);
}

/**
Prints command line help
 */
void help() {

    printf("\tUsage: weborf [OPTIONS]\n");
    printf("\tStart the weborf webserver\n\n");
#ifdef IPV6
    printf("\tCompiled for IPv6\n\n");
#else
    printf("\tCompiled for IPv4\n\n");
#endif

    printf("  -p, --port	followed by port number to listen\n");
    printf
    ("  -i, --ip	followed by IP address to listen (dotted format)\n");
    printf("  -b, --basedir	followed by absolute path of basedir\n");
    printf
    ("  -a, --auth    followed by absolute path of the program to handle authentication\n");
    printf
    ("  -x  --noexec  tells weborf to send each file instead of executing scripts\n");
    printf("  -I  --index   list of index files, comma-separated\n");
    printf
    ("  -V  --virtual list of virtualhosts in the form host=basedir, comma-separated\n");
    printf("  -u            followed by a valid uid\n");
    printf
    ("                If started by root weborf will use this user to read files and execute scripts\n");
    printf("  -d            run as a daemon\n");
    printf("  -h, --help	display this help and exit\n");
    printf("  -v, --version	print program version\n\n");

    printf("Default port is %s\n", PORT);
    printf("Default base directory is %s\n\n", BASEDIR);

    printf("Report bugs to <tiposchi@tiscali.it>\n");
    exit(0);
}

/**
Searching for easter eggs within the code isn't fair!
*/
void moo() {
    printf(" _____________________________________\n");
    printf("< Weborf ha i poteri della supermucca >\n");
    printf(" -------------------------------------\n");
    printf("        \\   ^__^\n");
    printf("         \\  (oo)\\_______\n");
    printf("            (__)\\       )\\/\\\n");
    printf("                ||----w |\n");
    printf("                ||     ||\n");
    exit(0);
}

/**
This functions set enviromental variables according to the data present in the HTTP request
*/
void setEnvVars(char *http_param) {				//Sets Enviroment vars
    if (http_param == NULL)
        return;

    char *lasts;
    char *param;
    int i;
    int p_len;

    //Removes the 1st part with the protocol
    param = strtok_r(http_param, "\r\n", &lasts);
    setenv("SERVER_PROTOCOL", param, true);

    //Cycles parameters
    while ((param = strtok_r(NULL, "\r\n", &lasts)) != NULL) {

        p_len = strlen(param);
        char *value = NULL;

        //Parses the parameter to split name from value
        for (i = 0; i < p_len; i++) {
            if (param[i] == ':' && param[i + 1] == ' ') {
                param[i] = '\0';
                value = &param[i + 2];
                break;
            }
        }
        char hparam[200];
        hparam[0] = 'H';
        hparam[1] = 'T';
        hparam[2] = 'T';
        hparam[3] = 'P';
        hparam[4] = '_';
        hparam[5] = '\0';
        strToUpper(param);	//Converts to upper case
        strReplace(param, "-", '_');
        strcat(hparam, param);
        setenv(hparam, value, true);
    }
}

/**
This function retrieves the value of an http field within the header
http_param is the string containing the header
parameter is the searched parameter
buf is the buffer where copy the value
size, maximum size of the buffer

Returns false if the parameter isn't found, or true otherwise
*/
bool
get_param_value(char *http_param, char *parameter, char *buf, int size) {
    char *val = strstr(http_param, parameter);	//Locates the requested parameter information

    if (val == NULL) {		//No such field
        return false;
    } else {			//Retrieves the field
        char *field_end = strstr(val, "\r\n");	//Searches the end of the parameter
        val += strlen(parameter) + 2;	//Moves the begin of the string to exclude the name of the field

        if ((field_end - val + 1) < size) {	//If the parameter's length is less than buffer's size
            memcpy(buf, val, field_end - val);
        } else {		//Parameter string is too long for the buffer
            return false;
        }
        buf[field_end - val] = 0;	//Ends the string within the destination buffer
    }
    return true;
}

/**
This function gets the name of the host from the var HTTP_HOST and sets the var
SERVER_ADDR using the ip obtained from the reverse lookup of the host.

This is used for cgi.

If the host var isn't set, it will return.

Since the host var can contain the port number too, it does a check to eliminate it,
before performing the lookup. It works with both ipv4 and 6.

@author Michal Ludvig <michal@logix.cz> (c) 2002, 2003, http://www.logix.cz/michal/devel/
Modified by Salvo 'LtWorf' Tomaselli
*/
int setIpEnv() {
    //Requested host name
    char *e_host = getenv("HTTP_HOST");
    if (e_host == NULL) {	//If no host is supplied in the request
        return -1;
    }
    //Since e_host can't be modified or the env var will change too, i have to copy it.
    char host[256];
    memmove(&host, e_host, strlen(e_host) + 1);

    if (host[0] == '[') {	//IPv6 address
        char *e = strstr(host, "]");
        e[0] = 0;
        delChar(host, 0, 1);
    } else {			//IPv4 or hostname
        char *e = strstr(host, ":");
        if (e != NULL)
            e[0] = 0;		//Removing port number
    }

    struct addrinfo hints, *res;
    int errcode;
    char addrstr[100];
    void *ptr = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags |= AI_CANONNAME;
    errcode = getaddrinfo(host, NULL, &hints, &res);
    if (errcode != 0) {
        return -1;
    }

    inet_ntop(res->ai_family, res->ai_addr->sa_data, addrstr, 100);

    switch (res->ai_family) {
    case AF_INET:
        ptr = &((struct sockaddr_in *) res->ai_addr)->sin_addr;
        break;
    case AF_INET6:
        ptr = &((struct sockaddr_in6 *) res->ai_addr)->sin6_addr;
        break;
    }

    inet_ntop(res->ai_family, ptr, addrstr, 100);
    setenv("SERVER_ADDR", addrstr, true);
    freeaddrinfo(res);
    return 0;
}
