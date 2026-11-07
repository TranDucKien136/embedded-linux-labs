/*
 * sensor_thread.c - Read sensor data and measure scheduling jitter
 * Author: Tran Duc Kien - SE183502
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <sched.h>
#include <sys/ioctl.h>
#include "sensor_thread.h"
#include "../driver/sms_sensor.h"

#define JITTER_MAX_SAMPLES 20000

/* Calculate time difference in microseconds */
static long timespec_diff_us(const struct timespec *a,
                             const struct timespec *b) {
    return (a->tv_sec - b->tv_sec) * 1000000L
         + (a->tv_nsec - b->tv_nsec) / 1000L;
}

static int cmp_long(const void *a, const void *b) {
    long x = *(const long *)a;
    long y = *(const long *)b;

    return (x > y) - (x < y);
}

static void jitter_report(long *j, size_t n, int use_fifo, unsigned long overruns) {
    size_t i;
    size_t p99_index;
    long min, max;
    long double sum = 0.0;
    double avg;
    size_t hist[4] = {0, 0, 0, 0};

    if (n == 0) {
        printf("JITTER REPORT: khong co mau\n");
        return;
    }

    min = j[0];
    max = j[0];

    /* min, max, sum, histogram */
    for (i = 0; i < n; i++) {
        if (j[i] < min)
            min = j[i];

        if (j[i] > max)
            max = j[i];

        sum += j[i];

        if (j[i] < 100)
            hist[0]++;
        else if (j[i] < 500)
            hist[1]++;
        else if (j[i] < 1000)
            hist[2]++;
        else
            hist[3]++;
    }

    avg = (double)(sum / n);

    /* Sap xep tang dan de tim P99 */
    qsort(j, n, sizeof(long), cmp_long);

    p99_index = (n * 99) / 100;

    /* Bao ve chi so trong pham vi mang */
    if (p99_index >= n)
        p99_index = n - 1;

    printf("\n===== JITTER REPORT (%s) =====\n",use_fifo ? "SCHED_FIFO prio 50" : "SCHED_OTHER");

    printf("Samples              : %zu\n", n);
    printf("Overruns             : %lu\n", overruns);
    printf("Min/Avg/Max/P99 (us) : %ld / %.2f / %ld / %ld\n",
           min, avg, max, j[p99_index]);

    printf("Histogram:\n");
    printf("  0-100us    : %zu (%.2f%%)\n",
           hist[0], 100.0 * hist[0] / n);
    printf("  100-500us  : %zu (%.2f%%)\n",
           hist[1], 100.0 * hist[1] / n);
    printf("  500-1000us : %zu (%.2f%%)\n",
           hist[2], 100.0 * hist[2] / n);
    printf("  >1ms       : %zu (%.2f%%)\n",
           hist[3], 100.0 * hist[3] / n);

    printf("========================================\n");
}

void *sensor_thread_func(void *arg) {
    struct sensor_args *a = (struct sensor_args *)arg;
    struct sensor_data d;
    struct timespec next;
    struct timespec actual;
    char line[128];
    long *jitter;
    size_t n_jit = 0;
    unsigned long overruns = 0;
    int fd;
    int rc;
    ssize_t n;

    /* Configure real-time scheduling if requested */
    if (a->use_fifo) {
        struct sched_param sp = { .sched_priority = 50 };

        rc = pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp);

        if (rc != 0) {
            fprintf(stderr, "WARN: SCHED_FIFO: %s\n", strerror(rc));
        }
    }

    /* Display the current scheduling policy */
    {
        int policy;
        struct sched_param current_sp;

        rc = pthread_getschedparam(pthread_self(), &policy, &current_sp);

        if (rc == 0) {
            if (policy == SCHED_FIFO) {
                printf("Sensor thread: SCHED_FIFO prio %d\n",
                       current_sp.sched_priority);
            } else {
                printf("Sensor thread: SCHED_OTHER\n");
            }
        }
    }

    /* Allocate jitter storage once, before the periodic loop */
    jitter = malloc(JITTER_MAX_SAMPLES * sizeof(*jitter));

    if (jitter == NULL) {
        fprintf(stderr, "WARN: cannot allocate jitter buffer\n");
        a->error = 1;
        *a->running = 0;
        return NULL;
    }

    /* Open sensor device */
    fd = open(a->dev_path, O_RDONLY);

    if (fd < 0) {
        perror("sensor_thread: open");
        free(jitter);
        a->error = 1;
        *a->running = 0;
        return NULL;
    }

    /* Configure sensor sampling interval */
    if (ioctl(fd, SMS_SET_INTERVAL, &a->interval_ms) < 0) {
        perror("WARN: sensor_thread ioctl SMS_SET_INTERVAL");
    }

    /* Initialize the first absolute wake-up time */
    if (clock_gettime(CLOCK_MONOTONIC, &next) == -1) {
        perror("sensor_thread: clock_gettime");
        close(fd);
        free(jitter);
        a->error = 1;
        *a->running = 0;
        return NULL;
    }

    while (*a->running) {
        /* Calculate the next absolute deadline */
        next.tv_nsec += (long)a->interval_ms * 1000000L;

        while (next.tv_nsec >= 1000000000L) {
            next.tv_nsec -= 1000000000L;
            next.tv_sec++;
        }

        /* Sleep until the absolute deadline */

    do {
        rc = clock_nanosleep(CLOCK_MONOTONIC,
                         TIMER_ABSTIME,
                         &next,
                         NULL);
        } while (rc == EINTR);

    if (rc != 0) {
            fprintf(stderr, "sensor_thread: clock_nanosleep: %s\n",
            strerror(rc));
            a->error = 1;
            *a->running = 0;
            break;
        }

        /* Measure wake-up jitter before reading the driver */
        if (clock_gettime(CLOCK_MONOTONIC, &actual) == -1) {
            perror("sensor_thread: clock_gettime actual");
            a->error = 1;
            *a->running = 0;
            break;
        }

        long jitter_us = timespec_diff_us(&actual, &next);

        if (n_jit < JITTER_MAX_SAMPLES)
            jitter[n_jit++] = jitter_us;

        if (jitter_us > (long)a->interval_ms * 1000L) {
            overruns++;
            next = actual;
        }

        if (!*a->running) {
            break;
        }

        /* Read sensor data */
        n = read(fd, line, sizeof(line) - 1);

        if (n < 0) {
            if (errno == EINTR || errno == EAGAIN) {
                continue;
            }

            perror("sensor_thread: read");
            a->error = 1;
            *a->running = 0;
            break;
        }

        if (n == 0) {
            continue;
        }

        line[n] = '\0';

        if (sscanf(line, "%lld,%f,%f",
                   &d.timestamp_ms,
                   &d.temperature,
                   &d.humidity) != 3) {
            fprintf(stderr, "WARN: invalid sensor data: %s\n", line);
            continue;
        }

        if (cb_push(a->cb, &d) < 0) {
            fprintf(stderr, "WARN: circular buffer full, sample dropped\n");
        }
    }

    /* Close the sensor device */
    close(fd);
    jitter_report(jitter, n_jit, a->use_fifo, overruns);
    free(jitter);

    return NULL;
}
