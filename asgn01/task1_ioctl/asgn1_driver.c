#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/string.h>
#include "asgn1_ioctl.h"

#define AS01_DEVICE_NAME "asgn1"
#define AS01_BUFFER_SIZE 1024
#define AS01_MAJOR_NUMBER 244
#define AS01_PREFIX "[KERNEL] "
#define AS01_VERSION "asgn1_driver v1.0-SE183502"


static struct asgn1_driver {
    char buffer[AS01_BUFFER_SIZE];
    int buffer_len;
    int mode;
    int open_count;
    int write_count;
    int read_count;
    int last_write_size;
    struct mutex lock;
} asgn1_drv = { .lock = __MUTEX_INITIALIZER(asgn1_drv.lock),
                .mode = AS01_MODE_ECHO, };

/**
 * asgn1_open - dinh nghia ham open cho device
 * @inode: con tro den struct inode cua device
 * @file: con tro den struct file cua device
 * kernel se goi ham nay khi nguoi dung mo device
 * dem va in ra log khi device duoc mo
 */
static int asgn1_open(struct inode *inode, struct file *file) {
    int count;
    mutex_lock(&asgn1_drv.lock);
    asgn1_drv.open_count++;
    count = asgn1_drv.open_count;
    mutex_unlock(&asgn1_drv.lock);
    pr_info(AS01_DEVICE_NAME ": device opened (open_count = %d)\n", count);
    return 0;
}

/**
 * asgn1_release - dinh nghia ham release cho device
 * @inode: con tro den struct inode cua device
 * @file: con tro den struct file cua device
 * kernel se goi ham nay khi nguoi dung dong device
 * in ra log khi device bi dong
 */
static int asgn1_release(struct inode *inode, struct file *file) {
    pr_info(AS01_DEVICE_NAME ": device closed\n");
    return 0;
}

/**
 * asgn1_read - dinh nghia ham read cho device
 * @file: con tro den struct file cua device
 * @ubuf: con tro den buffer nguoi dung de doc du lieu tu device
 * @count: so byte nguoi dung muon doc tu device
 * @ppos: con tro den vi tri doc hien tai trong buffer cua device
 * kernel se goi ham nay khi nguoi dung doc du lieu tu device
 * tra ve so byte da doc thanh cong, hoac -EFAULT neu co loi xay ra
 */
static ssize_t asgn1_read(struct file *file, char __user *ubuf, size_t count, loff_t *ppos) {
    size_t nbytes;
    mutex_lock(&asgn1_drv.lock);
    if (*ppos >= asgn1_drv.buffer_len) {
        mutex_unlock(&asgn1_drv.lock);
        return 0; // EOF
    }
    nbytes = min_t(size_t, count, (asgn1_drv.buffer_len - *ppos));
    if (copy_to_user(ubuf, asgn1_drv.buffer + *ppos, nbytes)) {
        mutex_unlock(&asgn1_drv.lock);
        return -EFAULT;
    }
    asgn1_drv.read_count++;
    *ppos += nbytes;
    pr_info(AS01_DEVICE_NAME ": read %zu bytes from device (read_count = %d)\n", nbytes, asgn1_drv.read_count);
    mutex_unlock(&asgn1_drv.lock);
    return nbytes;
}

/**
 * asgn1_write - dinh nghia ham write cho device
 * @file: con tro den struct file cua device
 * @ubuf: con tro den buffer nguoi dung de ghi du lieu vao device
 * @count: so byte nguoi dung muon ghi vao device
 * @ppos: khong su dung trong ham write
 * kernel se goi ham nay khi nguoi dung ghi du lieu vao device
 * tra ve count ke ca khi ghi bi cat 
 */
static ssize_t asgn1_write(struct file *file, const char __user *ubuf, size_t count, loff_t *ppos) {
    size_t nbytes;
    size_t plen;
    mutex_lock(&asgn1_drv.lock);
    if (asgn1_drv.mode == AS01_MODE_KERNEL) {
        plen = strlen(AS01_PREFIX);
    } else {
        plen = 0;
    }
    nbytes = min_t(size_t, count, (AS01_BUFFER_SIZE - plen));
    if (plen > 0) {
        memcpy(asgn1_drv.buffer, AS01_PREFIX, plen);
    }
    if (copy_from_user(asgn1_drv.buffer + plen, ubuf, nbytes)) {
        mutex_unlock(&asgn1_drv.lock);
        return -EFAULT;
    }
    asgn1_drv.buffer_len = plen + nbytes;
    asgn1_drv.write_count++;
    asgn1_drv.last_write_size = nbytes;
    pr_info(AS01_DEVICE_NAME ": wrote %zu bytes to device (write_count = %d, mode = %d)\n",
        nbytes, asgn1_drv.write_count, asgn1_drv.mode);
    if (nbytes < count) {
        pr_warn(AS01_DEVICE_NAME ": write truncated, only %zu bytes written (count = %zu)\n", nbytes, count);
    }
    mutex_unlock(&asgn1_drv.lock);
    return count; // return count even if truncated
}

static long asgn1_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
    struct as01_stats stats;
    int mode;
    long ret = 0;
    if (_IOC_TYPE(cmd) != AS01_MAGIC_NUMBER || _IOC_NR(cmd) > AS01_IOCTL_MAXNR) {
        return -ENOTTY; // Command not supported
    }
    mutex_lock(&asgn1_drv.lock);
    switch (cmd) {
        case AS01_RESET_BUFFER:
            asgn1_drv.buffer_len = 0;
            asgn1_drv.write_count = 0;
            asgn1_drv.read_count = 0;
            pr_info(AS01_DEVICE_NAME": ioctl RESET_BUFFER\n");
            break;
        case AS01_GET_STATS:
            stats.open_count = asgn1_drv.open_count;
            stats.write_count = asgn1_drv.write_count;
            stats.read_count = asgn1_drv.read_count;
            stats.buffer_len = asgn1_drv.buffer_len;
            stats.last_write_size = asgn1_drv.last_write_size;
            if (copy_to_user((void __user *)arg, &stats, sizeof(stats))) {
                ret = -EFAULT;
            }
            pr_info(AS01_DEVICE_NAME": ioctl GET_STATS\n");
            break;
        case AS01_SET_MODE:
            if (get_user(mode, (int __user *)arg)) {
                ret = -EFAULT;
            } else if (mode != AS01_MODE_KERNEL && mode != AS01_MODE_ECHO) {
                ret = -EINVAL; // Invalid argument
            } else {
                asgn1_drv.mode = mode;
                pr_info(AS01_DEVICE_NAME": ioctl SET_MODE: mode set to %d\n", mode);
            }
            break;
        case AS01_GET_VERSION:
            if (copy_to_user((void __user *)arg, AS01_VERSION, strlen(AS01_VERSION) + 1)) {
                ret = -EFAULT;
            }
            pr_info(AS01_DEVICE_NAME": ioctl GET_VERSION\n");
            break;
        default:          
            ret = -ENOTTY; // Command not supported
    }
    mutex_unlock(&asgn1_drv.lock);
    return ret;
}

static const struct file_operations asgn1_fops = {
    .owner = THIS_MODULE,
    .open = asgn1_open,
    .release = asgn1_release,
    .read = asgn1_read,
    .write = asgn1_write,
    .unlocked_ioctl = asgn1_ioctl,
};

static int __init asgn1_init(void) {
    int ret;
    ret = register_chrdev(AS01_MAJOR_NUMBER, AS01_DEVICE_NAME, &asgn1_fops);
    if (ret < 0) {
        pr_err(AS01_DEVICE_NAME ": failed to register major number %d (error %d)\n", AS01_MAJOR_NUMBER, ret);
        return ret;
    }
    pr_info(AS01_DEVICE_NAME ": registered char device with major number %d\n", AS01_MAJOR_NUMBER);
    pr_info(AS01_DEVICE_NAME ": version: %s\n", AS01_VERSION);
    return 0;
}

static void __exit asgn1_exit(void) {
    unregister_chrdev(AS01_MAJOR_NUMBER, AS01_DEVICE_NAME);
    pr_info(AS01_DEVICE_NAME ": Unloaded\n");
}

module_init(asgn1_init);
module_exit(asgn1_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tran Duc Kien - SE183502");
MODULE_DESCRIPTION("Char device driver for assignment 1");
