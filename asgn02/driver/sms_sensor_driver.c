/*
 * sms_sensor_driver.c - Driver simulate temperature and humidity
 * Author: Tran Duc Kien - SE183502
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/random.h>
#include <linux/ktime.h>
#include <linux/delay.h>
#include <linux/math64.h>
#include "sms_sensor.h"

/* Sensor limits*/
#define SMS_TEMP_MIN       150
#define SMS_TEMP_MAX       450
#define SMS_HUMID_MIN      300
#define SMS_HUMID_MAX      900
#define SMS_LINE_LEN       64
#define SMS_PROC_NAME      "sms_stats"

/* Sensor state */
struct sms_sensor_state {
    int read_count;
    int last_temp;
    int last_humid;
    int interval_ms;
    s64 last_read_ms;
    s64 load_time_ms;
    struct mutex lock;
};

/* Shared sensor state */
static struct sms_sensor_state sensor_state = {
    .interval_ms = SMS_DEFAULT_INTERVAL_MS,
    .lock = __MUTEX_INITIALIZER(sensor_state.lock),
};

static ssize_t sms_read(struct file *file,
                        char __user *buf,
                        size_t count,
                        loff_t *ppos) {
    char kbuf[SMS_LINE_LEN];
    int temp, humid, len;
    s64 now, wait;

    /* Check the sampling interval */
    mutex_lock(&sensor_state.lock);
    now = ktime_to_ms(ktime_get());
    wait = sensor_state.last_read_ms
           + sensor_state.interval_ms - now;
    mutex_unlock(&sensor_state.lock);

    /* Wait if the next sample is not ready */
    if (wait > 0) {
        if (file->f_flags & O_NONBLOCK)
            return -EAGAIN;

        if (msleep_interruptible((unsigned int)wait) != 0)
            return -EINTR;
    }

    /* Generate a new temperature and humidity sample */
    temp = SMS_TEMP_MIN +
           get_random_u32() % (SMS_TEMP_MAX - SMS_TEMP_MIN + 1);

    humid = SMS_HUMID_MIN +
            get_random_u32() % (SMS_HUMID_MAX - SMS_HUMID_MIN + 1);

    /* Update sensor state */
    mutex_lock(&sensor_state.lock);

    now = ktime_to_ms(ktime_get());
    sensor_state.last_temp = temp;
    sensor_state.last_humid = humid;

    if (now - sensor_state.last_read_ms
        < 2LL * sensor_state.interval_ms) {
        sensor_state.last_read_ms += sensor_state.interval_ms;
    } else {
        sensor_state.last_read_ms = now;
    }
    sensor_state.read_count++;

    mutex_unlock(&sensor_state.lock);

    /* Format the sample */
    len = snprintf(kbuf, sizeof(kbuf),
                   "%lld,%d.%d,%d.%d\n",
                   (long long)now,
                   temp / 10, temp % 10,
                   humid / 10, humid % 10);

    if (len < 0)
        return -EINVAL;

    if (len > count)
        return -EINVAL;

    /* Copy the sample to user space */
    if (copy_to_user(buf, kbuf, len))
        return -EFAULT;

    return len;
}

static int sms_proc_show(struct seq_file *m, void *v)
{
    int read_count, temp, humid;
    s64 load_time_ms, now;

    mutex_lock(&sensor_state.lock);

    read_count = sensor_state.read_count;
    temp = sensor_state.last_temp;
    humid = sensor_state.last_humid;
    load_time_ms = sensor_state.load_time_ms;

    mutex_unlock(&sensor_state.lock);

    now = ktime_to_ms(ktime_get());

    seq_printf(m, "read_count       : %d\n", read_count);
    seq_printf(m, "last_temperature : %d.%d C\n",temp / 10, temp % 10);
    seq_printf(m, "last_humidity    : %d.%d %%\n",humid / 10, humid % 10);
    seq_printf(m, "uptime           : %lld s\n",(long long)div_s64(now - load_time_ms, 1000));
    return 0;
}

static int sms_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, sms_proc_show, NULL);
}

static const struct proc_ops sms_proc_ops = {
    .proc_open    = sms_proc_open,
    .proc_read    = seq_read,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release,
};

static long sms_ioctl(struct file *file,
                      unsigned int cmd,
                      unsigned long arg)
{
    int val;

    if (_IOC_TYPE(cmd) != SMS_MAGIC)
        return -ENOTTY;

    switch (cmd) {
    case SMS_SET_INTERVAL:
        if (get_user(val, (int __user *)arg))
            return -EFAULT;

        if (val < 1 || val > 10000)
            return -EINVAL;

        mutex_lock(&sensor_state.lock);
        sensor_state.interval_ms = val;
        mutex_unlock(&sensor_state.lock);

        pr_info("sms_sensor: interval set to %d ms\n", val);
        return 0;

    default:
        return -ENOTTY;
    }
}

static const struct file_operations sms_fops = {
    .owner          = THIS_MODULE,
    .read           = sms_read,
    .unlocked_ioctl = sms_ioctl,
};

static struct miscdevice sms_misc = {
    .minor = MISC_DYNAMIC_MINOR,
    .name  = "sms_sensor",
    .fops  = &sms_fops,
};

static int __init sms_init(void) {
    int ret;

    sensor_state.load_time_ms = ktime_to_ms(ktime_get());

    ret = misc_register(&sms_misc);
    if (ret) {
        pr_err("sms_sensor: failed to register device\n");
        return ret;
    }

    ret = proc_create(SMS_PROC_NAME, 0444, NULL, &sms_proc_ops)
          ? 0 : -ENOMEM;

    if (ret) {
        misc_deregister(&sms_misc);
        pr_err("sms_sensor: failed to create proc entry\n");
        return ret;
    }

    pr_info("sms_sensor: loaded, minor = %d\n", sms_misc.minor);

    return 0;
}

static void __exit sms_exit(void) {
    remove_proc_entry(SMS_PROC_NAME, NULL);
    misc_deregister(&sms_misc);

    pr_info("sms_sensor: unloaded\n");
}

module_init(sms_init);
module_exit(sms_exit);

/* Modules */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tran Duc Kien - SE183502");
MODULE_DESCRIPTION("Simulated temperature and humidity sensor driver");
