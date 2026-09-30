#include "circbuf.h"
#include <stdlib.h>
#include <string.h>

void circbuf_init(circbuf_t *cb)
{
    memset(cb, 0, sizeof(*cb));
    pthread_mutex_init(&cb->mutex, NULL);
    pthread_cond_init(&cb->not_empty, NULL);
    pthread_cond_init(&cb->not_full, NULL);
}

void circbuf_destroy(circbuf_t *cb)
{
    //Καθάρισμα oυρας. 
    for (size_t i = 0; i < cb->count; i++) {
        size_t idx = (cb->head + i) % CIRCBUF_CAPACITY;
        free(cb->slots[idx]);
    }
    pthread_mutex_destroy(&cb->mutex);
    pthread_cond_destroy(&cb->not_empty);
    pthread_cond_destroy(&cb->not_full);
}

int circbuf_push(circbuf_t *cb, char *msg)
{
    pthread_mutex_lock(&cb->mutex);

    while (cb->count == CIRCBUF_CAPACITY && !cb->shutting_down)
        pthread_cond_wait(&cb->not_full, &cb->mutex);

    if (cb->shutting_down) {
        pthread_mutex_unlock(&cb->mutex);
        return -1;
    }

    cb->slots[cb->tail] = msg;
    cb->tail = (cb->tail + 1) % CIRCBUF_CAPACITY;
    cb->count++;

    pthread_cond_signal(&cb->not_empty);
    pthread_mutex_unlock(&cb->mutex);
    return 0;
}

char *circbuf_pop(circbuf_t *cb)
{
    pthread_mutex_lock(&cb->mutex);

    while (cb->count == 0 && !cb->shutting_down)
        pthread_cond_wait(&cb->not_empty, &cb->mutex);

    if (cb->count == 0 && cb->shutting_down) {
        pthread_mutex_unlock(&cb->mutex);
        return NULL;
    }

    char *msg = cb->slots[cb->head];
    cb->head = (cb->head + 1) % CIRCBUF_CAPACITY;
    cb->count--;

    pthread_cond_signal(&cb->not_full);
    pthread_mutex_unlock(&cb->mutex);
    return msg;
}

double circbuf_occupancy_pct(circbuf_t *cb)
{
    pthread_mutex_lock(&cb->mutex);
    double pct = (100.0 * (double)cb->count) / (double)CIRCBUF_CAPACITY;
    pthread_mutex_unlock(&cb->mutex);
    return pct;
}

void circbuf_shutdown(circbuf_t *cb)
{
    pthread_mutex_lock(&cb->mutex);
    cb->shutting_down = 1;
    pthread_cond_broadcast(&cb->not_empty);
    pthread_cond_broadcast(&cb->not_full);
    pthread_mutex_unlock(&cb->mutex);
}
