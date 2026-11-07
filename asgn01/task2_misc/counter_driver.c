
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/atomic.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#define COUNTER_NAME "counter"
#define COUNTER_PROC_NAME "counter_info"
#define COUNTER_BUF_LEN 16

static atomic_t counter = ATOMIC_INIT(0);

static atomic_t read_total = ATOMIC_INIT(0);

static atomic_t write_total = ATOMIC_INIT(0);

static ssize_t counter_read(struct file *file, char __user *ubuf,
                          size_t count, loff_t *ppos) {
    int val;
    int len;
    char kbuf[COUNTER_BUF_LEN];

    if (*ppos > 0)
        return 0;

    val = atomic_read(&counter);
    atomic_inc(&counter);
    atomic_inc(&read_total);

    len = snprintf(kbuf, sizeof(kbuf), "%d\n", val);

    if (len > count)
        return -EINVAL;

    if (copy_to_user(ubuf, kbuf, len))
        return -EFAULT;

    *ppos += len;

    pr_info(COUNTER_NAME ": read %d\n", val);

    return len;
}

static ssize_t counter_write(struct file *file, const char __user *ubuf,
                             size_t count, loff_t *ppos)
{
    char kbuf[COUNTER_BUF_LEN];
    int val;
    int ret;

    if (count >= sizeof(kbuf))
        return -EINVAL;

    if (copy_from_user(kbuf, ubuf, count))
        return -EFAULT;

    kbuf[count] = '\0';

    ret = kstrtoint(kbuf, 10, &val);
    if (ret)
        return ret;

    if (val < 0)
        return -EINVAL;

    atomic_set(&counter, val);
    atomic_inc(&write_total);

    pr_info(COUNTER_NAME ": set counter = %d\n", val);

    return count;
}

/*  Display information through /proc/counter_info */
static int counter_proc_show(struct seq_file *m, void *v)
{
    seq_printf(m, "counter     : %d\n", atomic_read(&counter));
    seq_printf(m, "read_total  : %d\n", atomic_read(&read_total));
    seq_printf(m, "write_total : %d\n", atomic_read(&write_total));

    return 0;
}

/*  Open proc file */
static int counter_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, counter_proc_show, NULL);
}

/*  proc_ops for kernel 5.15 */
static const struct proc_ops counter_proc_ops = {
    .proc_open    = counter_proc_open,
    .proc_read    = seq_read,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release,
};

/*  File operations for /dev/counter */
static const struct file_operations counter_fops = {
    .owner = THIS_MODULE,
    .read  = counter_read,
    .write = counter_write,
};

/*  Misc device */
static struct miscdevice counter_misc = {
    .minor = MISC_DYNAMIC_MINOR,
    .name  = COUNTER_NAME,
    .fops  = &counter_fops,
};

/*  Initialize module */
static int __init counter_init(void)
{
    int ret;

    ret = misc_register(&counter_misc);
    if (ret) {
        pr_err(COUNTER_NAME ": failed to register misc device\n");
        return ret;
    }

    if (!proc_create(COUNTER_PROC_NAME, 0444, NULL,
                     &counter_proc_ops)) {
        misc_deregister(&counter_misc);
        pr_err(COUNTER_NAME ": failed to create proc entry\n");
        return -ENOMEM;
    }

    pr_info(COUNTER_NAME ": registered, minor = %d\n",
            counter_misc.minor);

    return 0;
}

/*  Remove module */
static void __exit counter_exit(void)
{
    remove_proc_entry(COUNTER_PROC_NAME, NULL);
    misc_deregister(&counter_misc);

    pr_info(COUNTER_NAME ": unloaded\n");
}

/*  Register module initialization */
module_init(counter_init);
module_exit(counter_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tran Duc Kien - SE183502");
MODULE_DESCRIPTION("misc counter device for assignment 1");