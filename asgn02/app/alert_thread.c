/*
 * alert_thread.c - Receive and handle sensor alarms
 * Author: Tran Duc Kien - SE183502
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <mqueue.h>
#include <fcntl.h>
#include "alert_thread.h"
#include "sms_common.h"

void *alert_thread_func(void *arg) {
    mqd_t mq_alarm;
    mqd_t mq_log;

    struct alarm_msg m;
    struct log_msg lm;
    ssize_t n;

    static const char *names[] = {
        "TEMP_HIGH",
        "HUMID_HIGH",
        "TEMP_SPIKE"
    };

    int counts[3] = {0, 0, 0};
    int i;

    (void)arg;

    /* Open existing message queues */
    mq_alarm = mq_open(SMS_ALARM_QUEUE, O_RDONLY);

    if (mq_alarm == (mqd_t)-1) {
        perror("alert_thread: mq_open alarm");
        return NULL;
    }

    mq_log = mq_open(SMS_LOG_QUEUE, O_WRONLY);

    if (mq_log == (mqd_t)-1) {
        perror("alert_thread: mq_open log");
        mq_close(mq_alarm);
        return NULL;
    }

    /* Wait for alarm messages */
    while (1) {
        n = mq_receive(mq_alarm,
                       (char *)&m,
                       sizeof(m),
                       NULL);

        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }

            perror("alert_thread: mq_receive");
            break;
        }

        /* Stop signal: do not count it as an alarm */
        if (m.type == ALARM_STOP) {
            break;
        }

        /* Validate alarm type before accessing counts */
        if (m.type < ALARM_TEMP_HIGH ||
            m.type > ALARM_TEMP_SPIKE) {
            fprintf(stderr, "alert_thread: invalid alarm type\n");
            continue;
        }

        counts[m.type]++;

        /* Print alarm using the required format */
        fprintf(stderr,
                "[ALERT][%lld] TYPE: %s VALUE: %.1f THRESHOLD: %.1f\n",
                m.timestamp_ms,
                names[m.type],
                m.value,
                m.threshold);

        /* Forward the alert to the logger */
        memset(&lm, 0, sizeof(lm));
        snprintf(lm.text, sizeof(lm.text),
                 "ALERT %s value=%.1f",
                 names[m.type],
                 m.value);

        if (mq_send(mq_log,
                    (const char *)&lm,
                    sizeof(lm),
                    0) == -1) {
            perror("alert_thread: mq_send log");
        }
    }

    /* Print alert totals */
    fprintf(stderr, "\nAlert summary:\n");

    for (i = 0; i < 3; i++) {
        fprintf(stderr, "%s: %d\n", names[i], counts[i]);
    }

    mq_close(mq_alarm);
    mq_close(mq_log);

    return NULL;
}