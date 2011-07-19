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
#include <pthread.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <utime.h>

#define EVENT_SIZE  ( sizeof (struct inotify_event) )
#define BUF_LEN     ( 1024 * ( EVENT_SIZE + 16 ) )

#include "cachedir.h"
#include "mtime_update.h"

char **paths=NULL;              //Array used to store pointers to malloc()ated strings containing watched paths
size_t p_len=0;
int fd=-1;

/*
 * Watches for IN_CLOSE_WRITE because that doesn't change directory's mtime.
 * Also creating is important, so we can add new dirs to the watch.
 */
int m_events=IN_CLOSE_WRITE | IN_CREATE;
pthread_t thread_id;

/**
  * Returns true if the indicated path exists and is
  * a directory.
  * In case it is impossible to stat the path, false
  * will be returned. But this might just mean that
  * permissions aren't enough to perform the operation
  */
static bool mtime_isdir(char *path) {
    struct stat stbuf;
    if (stat(path,&stbuf)==0 && S_ISDIR(stbuf.st_mode))
        return true;
    return false;
}

/**
 * Changes the default set of events to listen to.
 * e is a mask, see man 7 inotify to know what events are allowed.
 *
 * By default it watches for IN_CLOSE_WRITE | IN_CREATE.
 *
 * If you change for not watching IN_CREATE, child directories
 * will not be automatically added for watch.
 *
 * You might want to change IN_CLOSE_WRITE to IN_MODIFY to
 * have a more correct kind of accesses.
 *
 * Changing the events affects the subsequent calls to mtime_watch_dir
 */
void mtime_set_events(int e) {
    m_events=e;
}

/**
 * Recoursively adds inotify watches over a directory and its leaves.
 * Only adds watches to directories and not other kind of files.
 * The caller must ensure that it is being called on a directory.
 *
 * This function can't be called after the thread has been spawned.
 * That will lead to race conditions.
 *
 * fd: inotify file descriptor used to add the watches
 * path: Path of the directory to watch
 * paths: array where every fd corresponds to the path being watched
 * p_len: maximum index for the array
 *
 * Returns 0 in case everything is correct.
 *
 */
int mtime_watch_dir(char *path) {

    int retval=0;

    if (!mtime_isdir(path)) return -1;

    int r = inotify_add_watch(fd,path, m_events);
    if (r==-1 || (unsigned int)r>=p_len) return -1;
    //printf("watching %s %d\n",path,r);

    //Recoursive scan
    DIR *dp = opendir(path);
    if (dp == NULL) {
        return -1;
    }

    paths[r]=strdup(path);
    char file[URI_LEN];

    struct dirent entry;
    struct dirent *result;
    int return_code;


    for (return_code=readdir_r(dp,&entry,&result); result!=NULL && return_code==0; return_code=readdir_r(dp,&entry,&result)) { //Cycles trough dir's elements
        //Avoids dir . and .. but not all hidden files
        if (entry.d_name[0]=='.' && (entry.d_name[1]==0 || (entry.d_name[1]=='.' && entry.d_name[2]==0)))
            continue;

        snprintf(file,URI_LEN,"%s%s/",path, entry.d_name);
        if (mtime_isdir(file)) {
            retval+= mtime_watch_dir(file);
        }

    }

    closedir(dp);
    return retval;
}

/**
 * Creates a thread to listen to events and modify the mtimes of the
 * related directory.
 *
 * NOTE: never launch two threads before first joining, or one of them
 * will not be joinable anymore.
 */
int mtime_spawn_listener() {
    pthread_attr_t t_attr;
    pthread_attr_init(&t_attr);
    //pthread_attr_setdetachstate(&t_attr, PTHREAD_CREATE_DETACHED);

    return pthread_create(&thread_id, &t_attr, mtime_listener, (void *)NULL);

}

/**
 * Terminates the thread listening for events
 *
 * NOTE: Never try to join a thread before launchin one with
 * mtime_spawn_listener()
 */
int mtime_join_listener() {
    pthread_cancel(thread_id);
    return pthread_join(thread_id,NULL);
}

/**
 * Listens for events and updates the mtime of the directory containing
 * the file that generated the event.
 */
void mtime_listener() {
    char buffer[BUF_LEN];
    int length;
    int i;

    for (;;) {
        length = read( fd, buffer, BUF_LEN );

        if ( length <= 0 ) return;

        i=0;
        while ( i < length ) {
            struct inotify_event *event = (struct inotify_event *) &buffer[i];
            if ( event->len ) {
                if (event->mask & (IN_CREATE | IN_ISDIR)) {
                    char file[URI_LEN];
                    snprintf(file,URI_LEN,"%s%s/",paths[event->wd],event->name);
                    mtime_watch_dir(file);
                } else {
                    //Update time on modifications, because on creation it is already done.
                    if (utime(paths[event->wd],NULL)!=0) {
                        //updating mtime failed, falling back to deleting cache items
                        cache_clean_element(paths[event->wd]);
                    }
                }

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

/**
 * Frees the allocated resources.
 * Never call this function before 1st allocating the resources.
 */
void mtime_free() {
    int i;
    for (i=0; i<MTIME_MAX_WATCH_DIRS; i++) {
        if (paths[i] != NULL) {
            free(paths[i]);
            inotify_rm_watch(fd,i);

        }
    }

    free(paths);
    close(fd);
}

/*int main( int argc, char **argv ) {
    int wd;

    mtime_init();


    mtime_watch_dir("/tmp/o/");
    mtime_watch_dir("/tmp/p/");

    //mtime_listener();
    mtime_spawn_listener();

    {
        int i;
        for (i=0;i<40;i++) {
            printf("...\n");
            sleep(3);
        }
    }

    printf("join %d\n",mtime_join_listener());
    mtime_free();

    exit( 0 );
}

*/
