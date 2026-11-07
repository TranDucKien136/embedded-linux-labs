/*
 * processor_thread.h - Sensor data processing thread interface
 * Author: Tran Duc Kien - SE183502
 */

#ifndef PROCESSOR_THREAD_H
#define PROCESSOR_THREAD_H
#include "circular_buffer.h"

/* Processor thread arguments */
struct processor_args {
    struct circular_buffer *cb;
    float temp_threshold;
    float humid_threshold;
    int verbose;
};

/* Processor thread entry point */
void *processor_thread_func(void *arg);

#endif
