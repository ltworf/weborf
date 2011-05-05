/*
Weborf
Copyright (C) 2011  Salvo "LtWorf" Tomaselli

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

#include <sys/inotify.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>










#define EVENT_SIZE  ( sizeof (struct inotify_event) )
#define BUF_LEN     ( 1024 * ( EVENT_SIZE + 16 ) )


#include "mtime_update.h"

char **paths=NULL;
size_t p_len=0;
int fd=-1;



/**
 * Recoursively adds inotify watches over a directory and its leaves.
 * Only adds watches to directories and not other kind of files.
 * The caller must ensure that it is being called on a directory.
 *
 * fd: inotify file descriptor used to add the watches
 * path: Path of the directory to watch
 * paths: array where every fd corresponds to the path being watched
 * p_len: maximum index for the array
 *
 * Returns 0 in case everything is correct.
 *
 */
static int mtime_watch_dir(char *path) {
    /*
     * Watches for IN_MODIFY because that is the only activity on file that
     * doesn't already change directory's mtime.
     * Adding and deleting aren't important to us.
     */
    int retval=0;
    int r = inotify_add_watch(fd,path, IN_MODIFY);
    if (r==-1 || r>=p_len) return -1;
    paths[r]=path;

    //Recoursive scan
    DIR *dp = opendir(path);
    char file[URI_LEN];

    struct dirent entry;
    struct dirent *result;
    int return_code;

    if (dp == NULL) {//Error, unable to send because header was already sent
        return -1;
    }

    for (return_code=readdir_r(dp,&entry,&result); result!=NULL && return_code==0; return_code=readdir_r(dp,&entry,&result)) { //Cycles trough dir's elements
        //Avoids dir . and .. but not all hidden files
        if (entry.d_name[0]=='.' && (entry.d_name[1]==0 || (entry.d_name[1]=='.' && entry.d_name[2]==0)))
            continue;

        int psize=snprintf(file,URI_LEN,"%s%s/",path, entry.d_name);
        char *mfile=malloc(psize+1);
        strcpy(mfile,file);

        struct stat stbuf;
        if (stat(file,&stbuf)==0 && S_ISDIR(stbuf.st_mode))
            retval+=mtime_watch_dir(mfile);

    }

    closedir(dp);
    return retval;
}


static void mtime_listener() {
    char buffer[BUF_LEN];
    int length;
    int i;

    for (;;) {
        
    printf("waiting events..\n");
    length = read( fd, buffer, BUF_LEN );
    printf("processing..\n");

    if ( length <= 0 ) return;

    i=0;
    while ( i < length ) {
        struct inotify_event *event = ( struct inotify_event * ) &buffer[ i ];
        if ( event->len ) {
            
            utime(paths[event->wd],NULL);
        }
        i += EVENT_SIZE + event->len;
    }
    }

}

/**
 * Allocates the necessary memory and structures for the use of the mtime_update
 * module.
 */
int mtime_init() {
    fd = inotify_init();
    if (fd==-1) return -1;
    
    paths=calloc(sizeof(char*),MTIME_MAX_WATCH_DIRS);
    if (paths==NULL) return -1;
    p_len=MTIME_MAX_WATCH_DIRS;
    return 0;
    
}

void mtime_free() {
    for (int i=0;i<MIME_MAX_WATCH_DIRS;i++) {
        if (paths[i] != NULL) {
        free(paths[i]);
        inotify_rm_watch(fd,i);
            
        }
    }
    
    free(paths);
    close(fd);
}

int main( int argc, char **argv ) {
    int wd;
    
    mtime_init();
    

    mtime_watch_dir("/tmp/o/");

    mtime_listener();
    mtime_free();



    exit( 0 );
}

/*int main( int argc, char **argv )
{
  int length, i = 0;
  int fd;
  int wd;
  char buffer[BUF_LEN];

  fd = inotify_init();

  if ( fd < 0 ) {
    perror( "inotify_init" );
  }

  wd = inotify_add_watch( fd, "/home/salvo/",
                         IN_MODIFY | IN_CREATE | IN_DELETE );
  length = read( fd, buffer, BUF_LEN );

  if ( length < 0 ) {
    perror( "read" );
  }

  while ( i < length ) {
    struct inotify_event *event = ( struct inotify_event * ) &buffer[ i ];
    if ( event->len ) {
      if ( event->mask & IN_CREATE ) {
        if ( event->mask & IN_ISDIR ) {
          printf( "The directory %s was created.\n", event->name );
        }
        else {
          printf( "The file %s was created.\n", event->name );
        }
      }
      else if ( event->mask & IN_DELETE ) {
        if ( event->mask & IN_ISDIR ) {
          printf( "The directory %s was deleted.\n", event->name );
        }
        else {
          printf( "The file %s was deleted.\n", event->name );
        }
      }
      else if ( event->mask & IN_MODIFY ) {
        if ( event->mask & IN_ISDIR ) {
          printf( "The directory %s was modified.\n", event->name );
        }
        else {
          printf( "The file %s was modified.\n", event->name );
        }
      }
    }
    i += EVENT_SIZE + event->len;
  }

  ( void ) inotify_rm_watch( fd, wd );
  ( void ) close( fd );

  exit( 0 );
}*/