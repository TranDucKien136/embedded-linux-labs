/*
 * circular_buffer.h - Thread-safe circular buffer for sensor data
 * Author: Tran Duc Kien - SE183502
 */

#ifndef CIRCULAR_BUFFER_H
#define CIRCULAR_BUFFER_H

#include <pthread.h>

/* Sensor sample structure */
struct sensor_data {
    long long timestamp_ms;
    float temperature;
    float humidity;
};

/* Thread-safe circular buffer */
struct circular_buffer {
    struct sensor_data *items;
    int capacity;
    int head;
    int tail;
    int size;
    unsigned long total_produced;
    unsigned long total_consumed;
    unsigned long overflow_count;
    int shutdown;

    pthread_mutex_t lock;
    pthread_cond_t not_empty;
};

/* Initialize the circular buffer */
int cb_init(struct circular_buffer *cb, int capacity);

/* Release buffer memory and synchronization objects */
void cb_destroy(struct circular_buffer *cb);

/* Add a sample; return -1 if the buffer is full */
int cb_push(struct circular_buffer *cb, const struct sensor_data *d);

/* Remove a sample; wait if empty, return -1 after shutdown */
int cb_pop(struct circular_buffer *cb, struct sensor_data *out);

/* Signal waiting threads to shut down */
void cb_shutdown(struct circular_buffer *cb);

/* Retrieve buffer statistics */
void cb_get_stats(struct circular_buffer *cb,
                  unsigned long *total_produced,
                  unsigned long *total_consumed,
                  unsigned long *overflow_count,
                  int *current_size);

#endif
