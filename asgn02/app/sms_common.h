/*
 * sms_common.h - Shared message queue definitions
 * Author: Tran Duc Kien - SE183502
 */

#ifndef SMS_COMMON_H
#define SMS_COMMON_H
#define SMS_ALARM_QUEUE  "/sms_alarm"
#define SMS_LOG_QUEUE    "/sms_log"
#define SMS_LOG_MSG_LEN  256

/* Alarm types */
enum alarm_type {
    ALARM_TEMP_HIGH = 0,
    ALARM_HUMID_HIGH,
    ALARM_TEMP_SPIKE,
    ALARM_STOP,
};

struct alarm_msg {
    enum alarm_type type;
    long long timestamp_ms;
    float value;
    float threshold;
};

/* Log message structure */
struct log_msg {
    char text[SMS_LOG_MSG_LEN];
};

#endif
