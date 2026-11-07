/*
 * processor_thread.c - Process sensor data and generate alarms and logs
 * Author: Tran Duc Kien - SE183502
 */

#include <stdio.h>
#include <math.h>
#include <mqueue.h>
#include <fcntl.h>
#include "processor_thread.h"
#include "sms_common.h"
#include <string.h>

/* Send an alarm message to the alert thread */
static void send_alarm(mqd_t q,
                       enum alarm_type t,
                       long long ts,
                       float v,
                       float threshold) {
    struct alarm_msg m;

    memset(&m, 0, sizeof(m));
    m.type = t;
    m.timestamp_ms = ts;
    m.value = v;
    m.threshold = threshold;

    if (mq_send(q, (const char *)&m, sizeof(m), 0) == -1) {
        perror("processor_thread: mq_send alarm");
    }
}

void *processor_thread_func(void *arg) {
    struct processor_args *a = (struct processor_args *)arg;
    mqd_t mq_alarm;
    mqd_t mq_log;

    struct sensor_data d;
    struct log_msg lm;

    float window_t[10];
    float window_h[10];

    int count = 0;
    int idx = 0;
    int i;

    float sum_t;
    float sum_h;
    float avg_t;
    float avg_h;

    float prev_temp = 0.0f;
    int have_prev = 0;

    mq_alarm = mq_open(SMS_ALARM_QUEUE, O_WRONLY);

    if (mq_alarm == (mqd_t)-1) {
        perror("processor_thread: mq_open alarm");
        return NULL;
    }

    mq_log = mq_open(SMS_LOG_QUEUE, O_WRONLY);

    if (mq_log == (mqd_t)-1) {
        perror("processor_thread: mq_open log");
        mq_close(mq_alarm);
        return NULL;
    }

    while (cb_pop(a->cb, &d) == 0) {
        /* Add the current sample to the 10-sample window */
        window_t[idx] = d.temperature;
        window_h[idx] = d.humidity;

        idx = (idx + 1) % 10;

        if (count < 10)
            count++;

        /* Calculate averages using available samples */
        sum_t = 0.0f;
        sum_h = 0.0f;

        for (i = 0; i < count; i++) {
            sum_t += window_t[i];
            sum_h += window_h[i];
        }

        avg_t = sum_t / count;
        avg_h = sum_h / count;

        /* Check high temperature */
        if (d.temperature > a->temp_threshold) {
            send_alarm(mq_alarm,
                       ALARM_TEMP_HIGH,
                       d.timestamp_ms,
                       d.temperature,
                       a->temp_threshold);
        }

        /* Check high humidity */
        if (d.humidity > a->humid_threshold) {
            send_alarm(mq_alarm,
                       ALARM_HUMID_HIGH,
                       d.timestamp_ms,
                       d.humidity,
                       a->humid_threshold);
        }

        /* Check sudden temperature spike */
        if (have_prev &&
            fabsf(d.temperature - prev_temp) > 5.0f) {
            send_alarm(mq_alarm,
                       ALARM_TEMP_SPIKE,
                       d.timestamp_ms,
                       fabsf(d.temperature - prev_temp),
                       5.0f);
        }

        prev_temp = d.temperature;
        have_prev = 1;

        /* Send the log message */
        memset(&lm, 0, sizeof(lm));
        snprintf(lm.text, sizeof(lm.text),
                 "%lld T=%.1f H=%.1f avgT=%.1f avgH=%.1f",
                 d.timestamp_ms,
                 d.temperature,
                 d.humidity,
                 avg_t,
                 avg_h);

        if (mq_send(mq_log, (const char *)&lm, sizeof(lm), 0) == -1) {
            perror("processor_thread: mq_send log");
        }

        /* Print the same log line when verbose mode is enabled */
        if (a->verbose) {
            printf("%s\n", lm.text);
            fflush(stdout);
        }
    }

    mq_close(mq_alarm);
    mq_close(mq_log);

    return NULL;
}
