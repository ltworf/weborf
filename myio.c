/*
Weborf
Copyright (C) 2010  Salvo "LtWorf" Tomaselli

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
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <errno.h>

#include "instance.h"
#include "types.h"
#include "myio.h"

/**
Copies count bytes from the file descriptor "from" to the
file descriptor "to".
It is possible to use lseek on the descriptors before calling
this function.
Will not close any descriptor
*/
int fd_copy(int from, int to, off_t count) {
    char *buf=malloc(FILEBUF);//Buffer to read from file
    int reads,wrote;

    if (buf==NULL) {
#ifdef SERVERDBG
        syslog(LOG_CRIT,"Not enough memory to allocate buffers");
#endif
        return ERR_NOMEM;//If no memory is available
    }

    //Sends file
    while (count>0 && (reads=read(from, buf, FILEBUF<count? FILEBUF:count ))>0) {
        count-=reads;
        wrote=write(to,buf,reads);
        if (wrote!=reads) { //Error writing to the descriptor
#ifdef SOCKETDBG
            syslog(LOG_ERR,"Unable to send %s: error writing to the file descriptor",connection_prop->strfile);
#endif
            break;
        }
    }

    free(buf);
    return 0;
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
Deletes a directory and its content.
This function is something like rm -rf

dir is the directory to delete
file is a buffer. Allocated outside because it
will be reused by every recoursive call.
Its size is file_s

Returns 0 on success
*/
int dir_remove(char * dir) {
    /*
    If it is a file, removes it
    Otherwise list the directory's content,
    then do a recoursive call and do
    rmdir on self
    */
    if (unlink(dir)==0)
        return 0;

    DIR *dp = opendir(dir); //Open dir
    struct dirent *entry;

    if (dp == NULL) {
        return 1;
    }

    char*file=malloc(PATH_LEN);//Buffer for path
    if (file==NULL)
        return ERR_NOMEM;

    //Cycles trough dir's elements
    while ((entry=readdir(dp)) != NULL) { //Cycles trough dir's elements

        //skips dir . and .. but not all hidden files
        if (entry->d_name[0]=='.' && (entry->d_name[1]==0 || (entry->d_name[1]=='.' && entry->d_name[2]==0)))
            continue;

        snprintf(file, PATH_LEN, "%s/%s", dir, entry->d_name);
        dir_remove(file);
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
    int retval=rename(source,dest);

    //Not the same device, doing a normal copy
    if (retval==-1 && errno==EXDEV) {
        retval=file_copy(source,dest);
        if (retval==0)
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
    char *buf=NULL;

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
    int retval;

    if ((retval=rename(source,dest))==0) {
        return 0;
    } else if (retval==-1 && errno==EXDEV) {
        return dir_move_copy(source,dest,MOVE);
    } else {
        return 1;
    }

}

/**
Moves or copies a directory, depending on the method used

Returns 0 on success
*/
int dir_move_copy (char* source, char* dest,int method) {
    struct stat f_prop; //File's property
    int retval=0;

    if (mkdir(dest,S_IRWXU | S_IRWXG | S_IRWXO)!=0) {//Attemps to create destination directory
        return ERR_FORBIDDEN;
    }

    DIR *dp = opendir(source); //Open dir
    struct dirent *entry;
    int return_code;


    if (dp == NULL) {
        return ERR_FILENOTFOUND;
    }

    char*src_file=malloc(PATH_LEN*2);//Buffer for path
    if (src_file==NULL)
        return ERR_NOMEM;
    char* dest_file=src_file+PATH_LEN;

    //Cycles trough dir's elements
    while ((entry=readdir(dp)) != NULL) {

        //skips dir . and .. but not all hidden files
        if (entry->d_name[0]=='.' && (entry->d_name[1]==0 || (entry->d_name[1]=='.' && entry->d_name[2]==0)))
            continue;

        snprintf(src_file,PATH_LEN,"%s/%s",source, entry->d_name);
        snprintf(dest_file,PATH_LEN,"%s/%s",dest, entry->d_name);

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
