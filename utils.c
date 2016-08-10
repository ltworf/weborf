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
#include "options.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <stdbool.h>
#include <netdb.h>
#include <dirent.h>
#include <unistd.h>
#include <syslog.h>
#include <signal.h>

#include "mystring.h"
#include "utils.h"
#include "embedded_auth.h"

/**
This function reads the directory dir, putting inside the html string an html
page with links to all the files within the directory.

Buffer for html must be allocated by the calling function.
bufsize is the size of the buffer allocated for html
parent is true when the dir has a parent dir
*/
int list_dir(connection_t *connection_prop, char *html, unsigned int bufsize, bool parent) {
    int pagesize=0; //Written bytes on the page
    int maxsize = bufsize - 1; //String's max size
    int printf_s;
    char *color; //Depending on row count chooses a background color
    char *measure; //contains measure unit for file's size (B, KiB, MiB)
    int counter = 0;

    char path[INBUFFER]; //Buffer to contain element's absolute path

    struct dirent **namelist;
    counter = scandir(connection_prop->strfile, &namelist, 0, alphasort);


    if (counter <0) { //Open not succesfull
        return -1;
    }

    //Specific header table)
    pagesize=printf_s=snprintf(html+pagesize,maxsize,"%s<table><tr><td></td><td><i>Name</i></td><td><i>Size</i></td><td><i>Last Modified</i></td></tr>",HTMLHEAD);
    maxsize-=printf_s;

    //Cycles trough dir's elements
    int i;
    struct tm ts;
    struct stat f_prop; //File's property
    char last_modified[URI_LEN];

    //Print link to parent directory, if there is any
    if (parent) {
        printf_s=snprintf(html+pagesize,maxsize,"<tr style=\"background-color: #DFDFDF;\"><td>d</td><td><a href=\"../\">../</a></td><td>-</td><td>-</td></tr>");
        maxsize-=printf_s;
        pagesize+=printf_s;
    }

    for (i=0; i<counter; i++) {
        //Skipping hidden files
        if (namelist[i]->d_name[0] == '.') {
            free(namelist[i]);
            continue;
        }

        snprintf(path, INBUFFER,"%s/%s", connection_prop->strfile, namelist[i]->d_name);

        //Stat on the entry

        stat(path, &f_prop);
        int f_mode = f_prop.st_mode; //Get's file's mode

        //get last modified
        localtime_r(&f_prop.st_mtime,&ts);
        strftime(last_modified,URI_LEN, "%a, %d %b %Y %H:%M:%S GMT", &ts);

        if (S_ISREG(f_mode)) { //Regular file

            //Table row for the file

            //Scaling the file's size
            unsigned long long int size = f_prop.st_size;
            if (size < 1024) {
                measure="B";
            } else if ((size = (size / 1024)) < 1024) {
                measure="KiB";
            } else if ((size = (size / 1024)) < 1024) {
                measure="MiB";
            } else {
                size = size / 1024;
                measure="GiB";
            }

            if (i % 2 == 0)
                color = "white";
            else
                color = "#EAEAEA";

            printf_s=snprintf(html+pagesize,maxsize,
                              "<tr style=\"background-color: %s;\"><td>f</td><td><a href=\"%s\">%s</a></td><td>%lld%s</td><td>%s</td></tr>\n",
                              color, namelist[i]->d_name, namelist[i]->d_name, (long long int)size, measure,last_modified);
            maxsize-=printf_s;
            pagesize+=printf_s;

        } else if (S_ISDIR(f_mode)) { //Directory entry
            //Table row for the dir
            printf_s=snprintf(html+pagesize,maxsize,
                              "<tr style=\"background-color: #DFDFDF;\"><td>d</td><td><a href=\"%s/\">%s/</a></td><td>-</td><td>%s</td></tr>\n",
                              namelist[i]->d_name, namelist[i]->d_name,last_modified);
            maxsize-=printf_s;
            pagesize+=printf_s;
        }

        free(namelist[i]);
    }

    free(namelist);

    printf_s=snprintf(html+pagesize,maxsize,"</table>%s",HTMLFOOT);
    pagesize+=printf_s;

    return pagesize;
}

/**
Prints version information
*/
void version() {
    printf("Weborf %s\n"
           "Copyright (C) 2007 Salvo 'LtWorf' Tomaselli.\n"
           "This is free software.  You may redistribute copies of it under the terms of\n"
           "the GNU General Public License <http://www.gnu.org/licenses/gpl.html>.\n"
           "There is NO WARRANTY, to the extent permitted by law.\n\n"

           "Written by Salvo 'LtWorf' Tomaselli and Salvo Rinaldi.\n"
           "Synchronized queue by Prof. Giuseppe Pappalardo.\n\n"
           "http://ltworf.github.io/weborf/\n", VERSION);
    exit(0);
}

void print_capabilities() {
    printf("version:" VERSION "\n");

    printf("ipv:"
#ifdef IPV6
    "6"
#else
    "4"
#endif
    "\n"
    );

    printf("webdav:"
#ifdef WEBDAV
    "yes"
#else
    "no"
#endif
    "\n"
    );

    printf("mime:"
#ifdef SEND_MIMETYPES
    "yes"
#else
    "no"
#endif
    "\n"
    );

    printf("embedded_auth:"
#ifdef EMBEDDED_AUTH
    "yes"
#else
    "no"
#endif
    "\n"
    );

    exit(0);
}

/**
Prints command line help
 */
void help() {

    printf("\tUsage: weborf [OPTIONS]\n"
           "\tStart the weborf webserver\n\n"
#ifdef IPV6
           "\tCompiled for IPv6\n"
#else
           "\tCompiled for IPv4\n"
#endif

#ifdef WEBDAV
           "\tHas webdav support\n"
#endif

#ifdef SEND_MIMETYPES
           "\tHas MIME support\n"
#endif

           "Default port is        %s\n"
           "Default base directory %s\n"
           "Signature used         %s\n\n", PORT,BASEDIR,SIGNATURE);

    printf("  -a, --auth    followed by absolute path of the program to handle authentication\n"
           "  -b, --basedir followed by absolute path of basedir\n"
           "  -C, --cache   sets the directory to use for cache files\n"
           "  -c, --cgi     list of cgi files and binary to execute them comma-separated\n"
           "  -d            run as a daemon\n"
           "  -h, --help    display this help and exit\n"
           "  -I, --index   list of index files, comma-separated\n"
           "  -i, --ip  followed by IP address to listen (dotted format)\n"
           "  -k, --caps    lists the capabilities of the binary\n"
           "  -m, --mime    sends content type header to clients\n"
           "  -p, --port    followed by port number to listen\n"
           "  -T  --inetd   must be specified when using weborf with inetd or xinetd\n"
           "  -t  --tar     will send the directories as .tar.gz files\n"
           "  -u,           followed by a valid uid\n"
           "                If started by root weborf will use this user to read files and execute scripts\n"
           "  -V, --virtual list of virtualhosts in the form host=basedir, comma-separated\n"
           "  -v, --version print program version\n"
           "  -x, --noexec  tells weborf to send each file instead of executing scripts\n\n"


           "Report bugs here https://bugs.launchpad.net/weborf\n"
           "or to " PACKAGE_BUGREPORT "\n");
    exit(0);
}

/**
Searching for easter eggs within the code isn't fair!
*/
void moo() {
    printf(" _____________________________________\n"
           "< Weborf ha i poteri della supermucca >\n"
           " -------------------------------------\n"
           "        \\   ^__^\n"
           "         \\  (oo)\\_______\n"
           "            (__)\\       )\\/\\\n"
           "                ||----w |\n"
           "                ||     ||\n");
    exit(0);
}

/**
 * This function prints the start disclaimer on stdout.
 * It wants the command line parameters
 * */
void print_start_disclaimer(int argc, char *argv[]) {
    printf("Weborf\n"
           "This program comes with ABSOLUTELY NO WARRANTY.\n"
           "This is free software, and you are welcome to redistribute it\n"
           "under certain conditions.\nFor details see the GPLv3 License.\n"
           "Run %s --help to see the options\n", argv[0]);
}

/**
 * Detaches the process from the shell,
 * it is re-implemented because it is not
 * included in POSIX
 *
 * It shouldn't be executed after launching
 * other threads. In that case the effects are
 * not specified.
 * */
void daemonize() {
    if (fork() == 0)
        signal(SIGHUP, SIG_IGN);
    else
        exit(0);
}

/**
This function retrieves the value of an http field within the header
http_param is the string containing the header
parameter is the searched parameter
buf is the buffer where copy the value
size, maximum size of the buffer
param_len =lenght of the parameter

Returns false if the parameter isn't found, or true otherwise
*/
bool get_param_value(char *http_param, char *parameter, char *buf, ssize_t size,ssize_t param_len) {
    char *val = strstr(http_param, parameter); //Locates the requested parameter information

    if (val == NULL) { //No such field
        return false;
    }

    /*
     * It is very important for this line to be here, for security reasons.
     * It moves the pointer forward, assuming "Field: Value\r\n"
     * If the field is malformed like "Field0\r\n" the subsequent strstr
     * will fail and the function will return false.
     * Moving this line after the next strstr would introduce a security
     * vulnerability.
     * The strstr will not cause a segfault because at this point the header
     * string must at least terminate with "\r\n\r", the last '\r' is changed to 0
     * so there is enough space to perform the operation
     * */
    val += param_len + 2; //Moves the begin of the string to exclude the name of the field

    char *field_end = strstr(val, "\r\n"); //Searches the end of the parameter
    if (field_end==NULL) {
        return false;
    }

    if ((field_end - val + 1) < size) { //If the parameter's length is less than buffer's size
        memcpy(buf, val, field_end - val);
    } else { //Parameter string is too long for the buffer
        return false;
    }
    buf[field_end - val] = 0; //Ends the string within the destination buffer

    return true;
}


