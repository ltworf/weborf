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
This function reads the directory dir, putting inside the html string an html
page with links to all the files within the directory.

Buffer for html must be allocated by the calling function.
bufsize is the size of the buffer allocated for html. To avoid buffer overflows.
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
    pagesize=printf_s=snprintf(html+pagesize,maxsize,"%s<table><tr><td></td><td>Name</td><td>Size</td></tr>",HTMLHEAD);
    maxsize-=printf_s;

    //Cycles trough dir's elements
    int i;
    for (i=0; i<counter; i++) {
        //Skipping hidden files, except for .. link
        if (namelist[i]->d_name[0] == '.' && (!(parent==true && namelist[i]->d_name[1] == '.' && namelist[i]->d_name[2] == '\0'))) {
            free(namelist[i]);
            continue;
        }

        snprintf(path, INBUFFER,"%s/%s", connection_prop->strfile, namelist[i]->d_name);

        //Stat on the entry
        struct stat f_prop; //File's property
        stat(path, &f_prop);
        int f_mode = f_prop.st_mode; //Get's file's mode

        if (S_ISREG(f_mode)) { //Regular file

            //Table row for the file

            //Scaling the file's size
            off_t size = f_prop.st_size;
            if (size < 1024) {
                measure="B";
            } else if ((size = (size / 1024)) < 1024) {
                measure="KiB";
            } else {
                size = size / 1024;
                measure="MiB";
            }

            if (i % 2 == 0)
                color = "white";
            else
                color = "#EAEAEA";

            printf_s=snprintf(html+pagesize,maxsize,
                              "<tr style=\"background-color: %s;\"><td>f</td><td><a href=\"%s\">%s</a></td><td>%u%s</td></tr>\n",
                              color, namelist[i]->d_name, namelist[i]->d_name, (int)size, measure);
            maxsize-=printf_s;
            pagesize+=printf_s;

        } else if (S_ISDIR(f_mode)) { //Directory entry
            //Table row for the dir
            printf_s=snprintf(html+pagesize,maxsize,
                              "<tr style=\"background-color: #DFDFDF;\"><td>d</td><td><a href=\"%s/\">%s/</a></td><td>-</td></tr>\n",
                              namelist[i]->d_name, namelist[i]->d_name);
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
Returns true if the specified file exists
*/
bool file_exists(char *file) {
    int fp = open(file, O_RDONLY);
    if (fp >= 0) { // exists
        close(fp);
        return true;
    }
    return false;
}

/**
Prints version information
*/
void version() {
    printf("Weborf %s\n", VERSION);
    printf("Copyright (C) 2007 Salvo 'LtWorf' Tomaselli.\n");
    printf("This is free software.  You may redistribute copies of it under the terms of\n");
    printf("the GNU General Public License <http://www.gnu.org/licenses/gpl.html>.\n");
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

#ifdef WEBDAV
    printf("\tHas webdav support\n\n");
#endif

    printf("  -p, --port	followed by port number to listen\n");
    printf("  -i, --ip	followed by IP address to listen (dotted format)\n");
    printf("  -b, --basedir	followed by absolute path of basedir\n");
    printf("  -a, --auth    followed by absolute path of the program to handle authentication\n");
    printf("  -x  --noexec  tells weborf to send each file instead of executing scripts\n");
    printf("  -I  --index   list of index files, comma-separated\n");
    printf("  -c  --cgi     list of cgi files and binary to execute them comma-separated\n");
    printf("  -V  --virtual list of virtualhosts in the form host=basedir, comma-separated\n");
    printf("  -u            followed by a valid uid\n");
    printf("                If started by root weborf will use this user to read files and execute scripts\n");
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
void setEnvVars(char *http_param) { //Sets Enviroment vars
    if (http_param == NULL)
        return;

    char *lasts;
    char *param;
    int i;
    int p_len;

    //Removes the 1st part with the protocol
    param = strtok_r(http_param, "\r\n", &lasts);
    setenv("SERVER_PROTOCOL", param, true);

    char hparam[200];
    hparam[0] = 'H';
    hparam[1] = 'T';
    hparam[2] = 'T';
    hparam[3] = 'P';
    hparam[4] = '_';

    //Cycles parameters
    while ((param = strtok_r(NULL, "\r\n", &lasts)) != NULL) {

        p_len = lasts-param-1;
        char *value = NULL;

        //Parses the parameter to split name from value
        for (i = 0; i < p_len; i++) {
            if (param[i] == ':' && param[i + 1] == ' ') {
                param[i] = '\0';
                value = &param[i + 2];
                break;
            }
        }
        strToUpper(param); //Converts to upper case
        strReplace(param, "-", '_');
        memccpy(hparam+5,param,'\0',195);
        setenv(hparam, value, true);
    }
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

    //Retrieves the field
    char *field_end = strstr(val, "\r\n"); //Searches the end of the parameter
    if (field_end==NULL)
        return false;
    
    val += param_len + 2; //Moves the begin of the string to exclude the name of the field

    if ((field_end - val + 1) < size) { //If the parameter's length is less than buffer's size
        memcpy(buf, val, field_end - val);
    } else { //Parameter string is too long for the buffer
        return false;
    }
    buf[field_end - val] = 0; //Ends the string within the destination buffer

    return true;
}


/**
Deletes a directory and its content.
This function is something like rm -rf

dir is the directory to delete
file is a buffer. Allocated outside because it
will be reused by every recoursive call.
It's size is file_s

Returns 0 on success
*/
int deep_rmdir(char * dir) {
    /*
    If it is a file, removes it
    Otherwise list the directory's content,
    then do a recoursive call and do
    rmdir on self
    */
    if (unlink(dir)==0)
        return 0;

    DIR *dp = opendir(dir); //Open dir
    struct dirent entry;
    struct dirent *result;
    int return_code;


    if (dp == NULL) {
        return 1;
    }

    char*file=malloc(PATH_LEN);//Buffer for path
    if (file==NULL)
        return ERR_NOMEM;

    //Cycles trough dir's elements
    for (return_code=readdir_r(dp,&entry,&result); result!=NULL && return_code==0; return_code=readdir_r(dp,&entry,&result)) { //Cycles trough dir's elements

        //skips dir . and .. but not all hidden files
        if (entry.d_name[0]=='.' && (entry.d_name[1]==0 || (entry.d_name[1]=='.' && entry.d_name[2]==0)))
            continue;

        snprintf(file,PATH_LEN,"%s/%s",dir, entry.d_name);
        deep_rmdir(file);
    }

    closedir(dp);
    free(file);
    return rmdir(dir);
}

#ifdef WEBDAV
/**
Moves a file. If it is on the same partition it will create a new link and delete the previous link.
Otherwise it will create a new copy and delete the old one.

Returns 0 on success.
*/
int file_move(char* source, char* dest) {
    int retval=link(source,dest);

    //Could link, file was on the same partition
    if (retval==0)
        goto escape;

    //Couldn't link, doing a copy
    retval=file_copy(source,dest);
escape:
    if (retval==0) {
        unlink(source);
    }
    return retval;
}

/**
Copies a file into another file

Returns 0 on success
*/
int file_copy(char* source, char* dest) {
    int fd_from=-1;
    int fd_to=-1;
    ssize_t read_,write_;
    int retval=0;
    char* buf=NULL;

    //Open destination file
    if ((fd_to=open(dest,O_WRONLY|O_CREAT,S_IRUSR|S_IWUSR))<0) {
        retval=ERR_FORBIDDEN;
        goto escape;
    }

    if ((fd_from=open(source,O_RDONLY | O_LARGEFILE))<0) {
        retval = ERR_FILENOTFOUND;
        goto escape;
    }

    buf=malloc(FILEBUF);//Buffer to read from file
    if (buf==NULL) {
        retval= ERR_NOMEM;
        goto escape;
    }

    while ((read_=read(fd_from,buf,FILEBUF))>0) {
        write_=write(fd_to,buf,read_);

        if (write_!=read_) {
            retval= ERR_BRKPIPE;
            break;
        }
    }

escape:
    free(buf);
    if (fd_from>=0) close(fd_from);
    if (fd_to>=0) close(fd_to);
    return retval;
}

/**This function copies a directory.
The destination directory will be created and
will be filled with the same content of the source directory

Returns 0 on success
*/
int dir_copy (char* source, char* dest) {
    return dir_move_copy(source,dest,COPY);
}

/**This function moves a directory.
The destination directory will be created and
will be filled with the same content of the source directory.
Then, the source directory will be deleted.

Returns 0 on success
*/
int dir_move(char* source, char* dest) {
    return dir_move_copy(source,dest,MOVE);
}

/**
Moves or copies a directory, depending on the method used

Returns 0 on success
*/
int dir_move_copy (char* source, char* dest,int method) {
    struct stat f_prop; //File's property
    int retval;

    if (mkdir(dest,S_IRWXU | S_IRWXG | S_IRWXO)!=0) {//Attemps to create destination directory
        return ERR_FORBIDDEN;
    }

    DIR *dp = opendir(source); //Open dir
    struct dirent entry;
    struct dirent *result;
    int return_code;


    if (dp == NULL) {
        return ERR_FILENOTFOUND;
    }

    char*src_file=malloc(PATH_LEN*2);//Buffer for path
    if (src_file==NULL)
        return ERR_NOMEM;
    char* dest_file=src_file+PATH_LEN;

    //Cycles trough dir's elements
    for (return_code=readdir_r(dp,&entry,&result); result!=NULL && return_code==0; return_code=readdir_r(dp,&entry,&result)) {

        //skips dir . and .. but not all hidden files
        if (entry.d_name[0]=='.' && (entry.d_name[1]==0 || (entry.d_name[1]=='.' && entry.d_name[2]==0)))
            continue;

        snprintf(src_file,PATH_LEN,"%s/%s",source, entry.d_name);
        snprintf(dest_file,PATH_LEN,"%s/%s",dest, entry.d_name);

        stat(src_file, &f_prop);
        if (S_ISDIR(f_prop.st_mode)) {//Directory
            retval=dir_move_copy(src_file,dest_file,method);
        } else {//File
            if (method==MOVE) {
                retval=file_move(src_file,dest_file);
            } else {
                retval=file_copy(src_file,dest_file);
            }
        }

        if (retval!=0)
            goto escape;
    }

escape:
    closedir(dp);
    free(src_file);

    //Removing directory after that its content has been moved
    if (retval==0 && method==MOVE) {
        return rmdir(source);
    }
    return retval;
}
#endif
