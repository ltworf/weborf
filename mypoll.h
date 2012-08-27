/*
Weborf
Copyright (C) 2012  Salvo "LtWorf" Tomaselli

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


#ifndef WEBORF_MYPOLL
#define WEBORF_MYPOLL

#include "types.h"
#include "options.h"

#ifdef HAVE_EPOLL_CREATE

#include <sys/epoll.h>

typedef int poll_t;


static inline poll_t mypoll_create(int size) {
    return epoll_create(size);
}

static inline int mypoll_ctl(poll_t epfd, int op, int fd, struct epoll_event *event) {
    return epoll_ctl(epfd, op, fd, event);
}

static inline int mypoll_wait(poll_t epfd, struct epoll_event *events, int maxevents, int timeout) {
    return epoll_wait(epfd, events, maxevents, timeout);
}

#else
//FIXME
#warning "epoll wrapper Not yet implemented" 
#endif


#endif
