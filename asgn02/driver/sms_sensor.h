#ifndef SMS_SENSOR_H
#define SMS_SENSOR_H

/*
 * sms_sensor.h - Header file for SMS sensor ioctl commands
 * Author: Tran Duc Kien - SE183502
 */

#include <linux/ioctl.h>
#define SMS_MAGIC 'Y'  /* Magic number for ioctl commands */
#define SMS_SET_INTERVAL _IOW(SMS_MAGIC, 0, int) /* Set sensor sampling interval */
#define SMS_DEFAULT_INTERVAL_MS 100  /* Default sampling interval in milliseconds */
#define SMS_DEV_PATH "/dev/sms_sensor"  /* Device file path */

#endif /* SMS_SENSOR_H */
