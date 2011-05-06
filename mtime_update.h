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

/**
 *
 * This module watches recourisvely different directory trees for changes on files.
 * When a file is changed (by default when a file opened for writing is closed)
 * the mtime and atime of the directory containing that file, will be updated
 * to the current time.
 *
 * It does not update all the mtimes and atimes recoursively. Just one directory.
 *
 * The current implementation only allows one instance of it. But it is not so
 * important to allow different instances because other directory trees can be
 * added for watch.
 *
 * It also provides methods for spawning a pthread that will perform the operations
 * in parallel, leaving the main program free to do other tasks.
 *
 * It requires inotify support (linux only), otherwise it will compile empty stubs
 * that do nothing at all.
 *
 * It is not reentrant.
 *
 * EXAMPLE:
 * mtime_init();    //Initializes internal data structures
 *
 *
 * mtime_watch_dir("/tmp/o/");
 * mtime_watch_dir("/tmp/p/");
 *
 * mtime_spawn_listener();  //Starts a thread to update mtimes
 *
 * do_something();
 *
 * mtime_join_listener();   //Terminates the listening thread
 * mtime_free();
 */

#ifndef WEBORF_MTIME_UPDATE_H
#define WEBORF_MTIME_UPDATE_H

int mtime_init();
void mtime_free();
int mtime_watch_dir(char *path);
int mtime_spawn_listener();
int mtime_join_listener();
void mtime_listener();
void mtime_set_events(int e);

#endif
