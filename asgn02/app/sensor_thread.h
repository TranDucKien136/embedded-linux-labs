/*
 * sensor_thread.h - Sensor reader thread interface
 * Author: Tran Duc Kien - SE183502
 */

#ifndef SENSOR_THREAD_H
#define SENSOR_THREAD_H

#include <signal.h>
#include "circular_buffer.h"

struct sensor_args {
    const char *dev_path;
    int interval_ms;
    struct circular_buffer *cb;
    volatile sig_atomic_t *running;
    int use_fifo;
    int error;
};

/* Sensor thread entry point */
void *sensor_thread_func(void *arg);

#endif