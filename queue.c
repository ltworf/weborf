/*
Weborf
Copyright (C) 2007 Giuseppe Pappalardo

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
@author Salvo "LtWorf" Tomaselli <tiposchi@tiscali.it>
@author Salvo Rinaldi <salvin@anche.no>
*/

#include "queue.h"

/**
Inits the syncronized queue, allocating memory.
Requires the syn_queue_t struct and the size of the queue itself.
To deallocate the queue, use the q_free function.
*/
int q_init(syn_queue_t * q, int size) {
    q->num = q->head = q->tail = 0;
    q->size = size;

    q->data = (int *) malloc(sizeof(int) * size);

    if (q->data == NULL) { //Error, unable to allocate memory
        return 1;
    }

    pthread_mutex_init(&q->mutex, NULL);
    pthread_cond_init(&q->for_space, NULL);
    pthread_cond_init(&q->for_data, NULL);
    q->n_wait_dt = q->n_wait_sp = 0;

    return 0;
}

/** Frees the memory taken by the queue.
Requires the pointer to the queue struct
*/
void q_free(syn_queue_t * q) {
    free(q->data);
}

int q_get(syn_queue_t * q, int *val) {
    pthread_mutex_lock(&q->mutex);
    while (q->num == 0) {
        q->n_wait_dt++;
        pthread_cond_wait(&q->for_data, &q->mutex);
    }
    *val = q->data[q->head]; //Sets the value

    q->head = (q->head + 1) % q->size; //Moves the head
    q->num--; //Reduces count of the queue

    /*if ((q->num == q->size) && (q->n_wait_sp > 0)) {
        q->n_wait_sp--;
        pthread_cond_signal(&q->for_space);
    } // unlock also needed after signal*/

    pthread_mutex_unlock(&q->mutex); //   or threads blocked on wait
    return 0; //   will not proceed
}


int q_put(syn_queue_t * q, int val) {
    pthread_mutex_lock(&q->mutex);

    //Wakes up a sleeping thread
    if (q->n_wait_dt > 0) {
        q->n_wait_dt--;
        pthread_cond_signal(&q->for_data);
    } // unlock also needed after signal

    //Fails if queue is full
    if (q->num == q->size) {
        pthread_mutex_unlock(&q->mutex); // or threads blocked on wait
        return 1; // will not proceed
        //while (q->num == q->size) {
        //q->n_wait_sp++;
        //pthread_cond_wait(&q->for_space, &q->mutex);
    }
    q->data[q->tail] = val; //Set the data in position

    q->tail = (q->tail + 1) % q->size; //Moves the tail

    q->num++; //Increases count of filled positions

    pthread_mutex_unlock(&q->mutex); // or threads blocked on wait
    return 0; // will not proceed
}
