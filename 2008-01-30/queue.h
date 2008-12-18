/*
Weborf
Copyright (C) 2007  Giuseppe Pappalardo

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
 
@author Giuseppe Pappalardo <pappalardo@dmi.unict.it>
 */
#include <stdlib.h>
#include <pthread.h>

typedef struct {
	int	num, size;
	int head, tail;
	int * data;
	pthread_mutex_t mutex;
	pthread_cond_t for_space, for_data;
	int n_wait_sp, n_wait_dt;
} syn_queue_t;

int q_init(syn_queue_t * q, int size);
int q_put(syn_queue_t * q, int val);
int q_get(syn_queue_t * q, int * val);
void q_free(syn_queue_t * q);
