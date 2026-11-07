/*
 * logger_thread.h - Logger thread interface
 * Author: Tran Duc Kien - SE183502
 */

#ifndef LOGGER_THREAD_H
#define LOGGER_THREAD_H

/* Logger thread arguments */
struct logger_args {
    const char *log_path;
};

/* Logger thread entry point */
void *logger_thread_func(void *arg);

#endif