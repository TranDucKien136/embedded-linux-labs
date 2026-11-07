/*
 * logger_thread.c - Write log messages to a file
 * Author: Tran Duc Kien - SE183502
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <mqueue.h>
#include <time.h>
#include <sys/stat.h>

#include "logger_thread.h"
#include "sms_common.h"

#define LOG_MAX_SIZE (1024L * 1024L)
#define FLUSH_INTERVAL 5

void *logger_thread_func(void *arg) {
    struct logger_args *a = (struct logger_args *)arg;

    mqd_t mq_log;
    struct log_msg lm;
    struct timespec timeout;

    FILE *fp;
    char timestamp[32];
    char *rotated_path;

    time_t now;
    time_t last_flush;

    struct tm tm_info;
    ssize_t n;
    long file_size;

    if (a == NULL || a->log_path == NULL) {
        fprintf(stderr, "logger_thread: invalid arguments\n");
        return NULL;
    }

    /* Open the log file in append mode */
    fp = fopen(a->log_path, "a");

    if (fp == NULL) {
        perror("logger_thread: fopen");
        return NULL;
    }

    /* Prepare the rotated log filename */
    rotated_path = malloc(strlen(a->log_path) + 3);

    if (rotated_path == NULL) {
        perror("logger_thread: malloc");
        fclose(fp);
        return NULL;
    }

    snprintf(rotated_path,
             strlen(a->log_path) + 3,
             "%s.1",
             a->log_path);

    /* Open the existing log message queue */
    mq_log = mq_open(SMS_LOG_QUEUE, O_RDONLY);

    if (mq_log == (mqd_t)-1) {
        perror("logger_thread: mq_open");
        free(rotated_path);
        fclose(fp);
        return NULL;
    }

    last_flush = time(NULL);

    while (1) {
        /* Set an absolute CLOCK_REALTIME timeout one second ahead */
        if (clock_gettime(CLOCK_REALTIME, &timeout) == -1) {
            perror("logger_thread: clock_gettime");
            break;
        }

        timeout.tv_sec += 1;

        n = mq_timedreceive(mq_log,
                            (char *)&lm,
                            sizeof(lm),
                            NULL,
                            &timeout);

        if (n < 0) {
            if (errno == EINTR || errno == ETIMEDOUT) {
                /* Continue to the periodic flush check */
            } else {
                perror("logger_thread: mq_timedreceive");
                break;
            }
        } else if (n > 0) {
            /* Ensure the received text is null-terminated */
            lm.text[sizeof(lm.text) - 1] = '\0';

            /* Stop signal from main */
            if (strcmp(lm.text, "__STOP__") == 0) {
                break;
            }

            /* Generate a full date and time */
            now = time(NULL);

            if (localtime_r(&now, &tm_info) == NULL) {
                fprintf(stderr, "logger_thread: localtime_r failed\n");
                continue;
            }

            if (strftime(timestamp,
                         sizeof(timestamp),
                         "%Y-%m-%d %H:%M:%S",
                         &tm_info) == 0) {
                fprintf(stderr, "logger_thread: strftime failed\n");
                continue;
            }

            /* Write timestamp and message */
            if (fprintf(fp, "[%s] %s\n",
                        timestamp, lm.text) < 0) {
                perror("logger_thread: fprintf");
                break;
            }

            /* Check whether the log file exceeds 1 MiB */
            file_size = ftell(fp);

            if (file_size < 0) {
                perror("logger_thread: ftell");
                break;
            }

            if (file_size > LOG_MAX_SIZE) {
                if (fflush(fp) == EOF) {
                    perror("logger_thread: fflush before rotation");
                    break;
                }

                if (fclose(fp) == EOF) {
                    perror("logger_thread: fclose before rotation");
                    fp = NULL;
                    break;
                }

                fp = NULL;

                if (rename(a->log_path, rotated_path) == -1) {
                    perror("logger_thread: rename");
                    fp = fopen(a->log_path, "a");
                } else {
                    /* Start a new log file */
                    fp = fopen(a->log_path, "w");
                }

                if (fp == NULL) {
                    perror("logger_thread: reopen log file");
                    break;
                }
            }
        }

        /* Flush buffered data */
        now = time(NULL);

        if (now - last_flush >= FLUSH_INTERVAL) {
            if (fflush(fp) == EOF) {
                perror("logger_thread: periodic fflush");
                break;
            }

            last_flush = now;
        }
    }

    /* Flush and release resources */
    if (fp != NULL) {
        if (fflush(fp) == EOF) {
            perror("logger_thread: final fflush");
        }

        fclose(fp);
    }

    mq_close(mq_log);
    free(rotated_path);

    return NULL;
}