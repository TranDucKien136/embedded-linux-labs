/*
 * Sensor Monitoring System application
 * Author: Tran Duc Kien - SE183502
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <mqueue.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include "circular_buffer.h"
#include "sensor_thread.h"
#include "processor_thread.h"
#include "alert_thread.h"
#include "logger_thread.h"
#include "sms_common.h"
#include "../driver/sms_sensor.h"

#define SMS_LOG_PATH "/tmp/sms.log"

static volatile sig_atomic_t running = 1;

/* Ctrl+C */
static void on_sigint(int sig) {
    (void)sig;
    running = 0;
}

int main(int argc, char *argv[]) {
    int interval = 500;
    int duration = 60;
    int verbose = 0;
    int use_fifo = 0;
    int opt;
    int rc;
    unsigned long produced, consumed, overflow;
    int size;
    float temp_th = 40.0f;
    float humid_th = 80.0f;
    const char *dev_path = SMS_DEV_PATH;
    struct sigaction sa;
    struct mq_attr attr;
    mqd_t mq_alarm, mq_log;
    struct circular_buffer cb;
    struct sensor_args sarg;
    struct processor_args parg;
    struct logger_args larg;
    pthread_t t_sensor, t_proc, t_alert, t_logger;
    time_t start;

    while ((opt = getopt(argc, argv, "i:t:h:d:vrD:")) != -1) {
        switch (opt) {
        case 'i':
            interval = atoi(optarg);
            break;

        case 't':
            temp_th = (float)atof(optarg);
            break;

        case 'h':
            humid_th = (float)atof(optarg);
            break;

        case 'd':
            duration = atoi(optarg);
            break;

        case 'v':
            verbose = 1;
            break;

        case 'r':
            use_fifo = 1;
            break;

        case 'D':
            dev_path = optarg;
            break;

        default:
            fprintf(stderr,
                    "Usage: %s [-i interval_ms] [-t temp_threshold] "
                    "[-h humid_threshold] [-d duration_sec] [-v] [-r] "
                    "[-D device_path]\n",
                    argv[0]);
            return 2;
        }
    }

    /* Validate command-line options */
    if (interval <= 0 || duration < 0) {
        fprintf(stderr,
                "Usage: %s [-i interval_ms] [-t temp_threshold] "
                "[-h humid_threshold] [-d duration_sec] [-v] [-r] "
                "[-D device_path]\n",
                argv[0]);
        return 2;
    }

    /* Install SIGINT handler */
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_sigint;
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction");
        return 1;
    }

    /* Lock process memory to reduce paging latency */
    if (use_fifo) {
        if (mlockall(MCL_CURRENT | MCL_FUTURE) == -1)
        perror("mlockall");
    }

    FILE *log_check = fopen(SMS_LOG_PATH, "a");

    if (log_check == NULL) {
        perror("sms_app: cannot open log file");
        return 1;
    }

    fclose(log_check);

    mq_unlink(SMS_ALARM_QUEUE);
    mq_unlink(SMS_LOG_QUEUE);

    memset(&attr, 0, sizeof(attr));
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = sizeof(struct alarm_msg);

    mq_alarm = mq_open(SMS_ALARM_QUEUE,O_CREAT | O_RDWR,0644,&attr);

    if (mq_alarm == (mqd_t)-1) {
        perror("mq_open SMS_ALARM_QUEUE");
        return 1;
    }

    attr.mq_msgsize = sizeof(struct log_msg);

    mq_log = mq_open(SMS_LOG_QUEUE,O_CREAT | O_RDWR,0644,&attr);

    if (mq_log == (mqd_t)-1) {
        perror("mq_open SMS_LOG_QUEUE");
        mq_close(mq_alarm);
        mq_unlink(SMS_ALARM_QUEUE);
        return 1;
    }

    if (cb_init(&cb, 64) != 0) {
        fprintf(stderr, "Failed to initialize circular buffer\n");

        mq_close(mq_alarm);
        mq_close(mq_log);
        mq_unlink(SMS_ALARM_QUEUE);
        mq_unlink(SMS_LOG_QUEUE);

        return 1;
    }

    /* Configure sensor thread */
    /* Configure sensor thread */
    sarg.dev_path = dev_path;
    sarg.interval_ms = interval;
    sarg.cb = &cb;
    sarg.running = &running;
    sarg.use_fifo = use_fifo;
    sarg.error = 0;

    /* Configure processor thread */
    parg.cb = &cb;
    parg.temp_threshold = temp_th;
    parg.humid_threshold = humid_th;
    parg.verbose = verbose;

    /* Configure logger thread */
    larg.log_path = SMS_LOG_PATH;

    /* Start logger thread */
    rc = pthread_create(&t_logger, NULL, logger_thread_func, &larg);

    if (rc != 0) {
        fprintf(stderr, "pthread_create logger: %s\n", strerror(rc));
        return 1;
    }

    /* Start alert thread */
    rc = pthread_create(&t_alert, NULL, alert_thread_func, NULL);

    if (rc != 0) {
        fprintf(stderr, "pthread_create alert: %s\n", strerror(rc));
        return 1;
    }

    /* Start processor thread */
    rc = pthread_create(&t_proc, NULL, processor_thread_func, &parg);

    if (rc != 0) {
        fprintf(stderr, "pthread_create processor: %s\n", strerror(rc));
        return 1;
    }

    /* Start sensor thread */
    rc = pthread_create(&t_sensor, NULL, sensor_thread_func, &sarg);

    if (rc != 0) {
        fprintf(stderr, "pthread_create sensor: %s\n", strerror(rc));
        return 1;
    }

    start = time(NULL);

    while (running &&
           (duration == 0 || time(NULL) - start < duration)) {
        sleep(1);
    }

    /*stop sensor*/
    running = 0;
    pthread_join(t_sensor, NULL);

    /*stop circular buffer*/
    cb_shutdown(&cb);
    pthread_join(t_proc, NULL);

    /*stop arlet*/
    struct alarm_msg stop_a;
    memset(&stop_a, 0, sizeof(stop_a));
    stop_a.type = ALARM_STOP;

    if (mq_send(mq_alarm, (const char *)&stop_a,
                sizeof(stop_a), 0) == -1) {
        perror("mq_send ALARM_STOP");
    }

    pthread_join(t_alert, NULL);

    /* Stop logger  */
    struct log_msg stop_l;
    memset(&stop_l, 0, sizeof(stop_l));
    strcpy(stop_l.text, "__STOP__");

    if (mq_send(mq_log, (const char *)&stop_l,
                sizeof(stop_l), 0) == -1) {
        perror("mq_send __STOP__");
    }

    pthread_join(t_logger, NULL);

    /* Display final statistics */
    cb_get_stats(&cb, &produced, &consumed, &overflow, &size);

    printf("\n========== THONG BAO TONG KET ==========\n");
    printf("Uptime:           %ld seconds\n",(long)(time(NULL) - start));
    printf("Samples produced: %lu\n", produced);
    printf("Samples consumed: %lu\n", consumed);
    printf("Buffer overflows: %lu\n", overflow);
    printf("Samples remaining: %d\n", size);
    printf("=================================\n");

    /* Cleanup */
    cb_destroy(&cb);
    mq_close(mq_alarm);
    mq_close(mq_log);
    mq_unlink(SMS_ALARM_QUEUE);
    mq_unlink(SMS_LOG_QUEUE);

    return sarg.error ? 1 : 0;
}
