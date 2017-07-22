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
@author Giuseppe Pappalardo <pappalardo@dmi.unict.it>

*/

#ifndef WEBORF_LISTENER_H
#define WEBORF_LISTENER_H


#define NOMEM 7

void init_threads(unsigned int count);
void quit();
void print_queue_status();
void set_authsocket(char *);
void chn_thread_count(int val);
void set_new_uid(int uid);
void set_new_gid(int gid);

#endif
