/*
 * circular_buffer.c - Thread-safe circular buffer implementation
 * Author: Tran Duc Kien - SE183502
 */

#include <stdlib.h>
#include "circular_buffer.h"

/* Initialize the circular buffer */
int cb_init(struct circular_buffer *cb, int capacity) {
    cb->items = malloc(capacity * sizeof(struct sensor_data));

    if (cb->items == NULL)
        return -1;

    cb->capacity = capacity;
    cb->head = 0;
    cb->tail = 0;
    cb->size = 0;
    cb->total_produced = 0;
    cb->total_consumed = 0;
    cb->overflow_count = 0;
    cb->shutdown = 0;

    pthread_mutex_init(&cb->lock, NULL);
    pthread_cond_init(&cb->not_empty, NULL);

    return 0;
}

/* Destroy the circular buffer */
void cb_destroy(struct circular_buffer *cb) {
    free(cb->items);
    cb->items = NULL;

    pthread_mutex_destroy(&cb->lock);
    pthread_cond_destroy(&cb->not_empty);
}

/* Push a sample without waiting when the buffer is full */
int cb_push(struct circular_buffer *cb, const struct sensor_data *d) {
    pthread_mutex_lock(&cb->lock);

    if (cb->size == cb->capacity) {
        cb->overflow_count++;
        pthread_mutex_unlock(&cb->lock);
        return -1;
    }
    cb->items[cb->head] = *d;
    cb->head = (cb->head + 1) % cb->capacity;
    cb->size++;
    cb->total_produced++;

    pthread_cond_signal(&cb->not_empty);
    pthread_mutex_unlock(&cb->lock);

    return 0;
}

/* Pop a sample, waiting while the buffer is empty */
int cb_pop(struct circular_buffer *cb, struct sensor_data *out) {
    pthread_mutex_lock(&cb->lock);

    while (cb->size == 0 && !cb->shutdown) {
        pthread_cond_wait(&cb->not_empty, &cb->lock);
    }
    if (cb->size == 0) {
        pthread_mutex_unlock(&cb->lock);
        return -1;
    }
    *out = cb->items[cb->tail];
    cb->tail = (cb->tail + 1) % cb->capacity;
    cb->size--;
    cb->total_consumed++;

    pthread_mutex_unlock(&cb->lock);

    return 0;
}

/* Shut down the buffer and wake waiting threads */
void cb_shutdown(struct circular_buffer *cb) {
    pthread_mutex_lock(&cb->lock);
    cb->shutdown = 1;
    pthread_cond_broadcast(&cb->not_empty);
    pthread_mutex_unlock(&cb->lock);
}

/* Retrieve buffer statistics */
void cb_get_stats(struct circular_buffer *cb,
                  unsigned long *total_produced,
                  unsigned long *total_consumed,
                  unsigned long *overflow_count,
                  int *current_size) {
    pthread_mutex_lock(&cb->lock);

    *total_produced = cb->total_produced;
    *total_consumed = cb->total_consumed;
    *overflow_count = cb->overflow_count;
    *current_size = cb->size;

    pthread_mutex_unlock(&cb->lock);
}
