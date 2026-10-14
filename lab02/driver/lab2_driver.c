/*
 * lab2_driver.c
 * LAB-02: Embedded Linux Systems
 *
 * Character device driver minh hoa:
 *  - Vong doi kernel module (init/exit)
 *  - Character device voi file_operations (open/read/write/release)
 *  - Giao tiep userspace qua /proc (procfs) va /sys (sysfs)
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/mutex.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Embedded Linux Lab");
MODULE_DESCRIPTION("LAB-02: Simple Character Device Driver");
MODULE_VERSION("1.0");

#define DEVICE_NAME     "lab2"
#define CLASS_NAME      "lab2_class"
#define BUFFER_SIZE     1024
#define DRIVER_MAJOR    240
/* 240-254: reserved for local/experimental use (xem Documentation/admin-guide/devices.txt) */

/* ===== Bien toan cuc va bo dem noi bo ===== */
static int    major_number = DRIVER_MAJOR;
static char   device_buffer[BUFFER_SIZE];  /* bo dem du lieu trong kernel, dung chung cho moi fd */
static int    buffer_len = 0;
static int    open_count = 0;
static struct class  *lab2_class  = NULL;
static struct device *lab2_device = NULL;

/*
 * device_buffer la tai nguyen chia se giua nhieu process (co the mo /dev/lab2
 * dong thoi tu nhieu shell/chuong trinh khac nhau). Mutex dam bao chi mot
 * thread duoc sua doi buffer tai mot thoi diem, tranh race condition khi
 * hai write() xay ra dong thoi (vd: process A dang ghi do dai buffer_len
 * thi process B ghi de len giua luc A chua xong -> du lieu bi xen lan,
 * khong khop voi noi dung thuc te).
 */
static DEFINE_MUTEX(lab2_mutex);

/* ===== PROCFS Implementation =====
 * procfs expose thong tin dang text, chi doc, thong qua seq_file API.
 * seq_file giai quyet van de doc du lieu lon hon mot page: no tu quan ly
 * vi tri doc (iterator) va goi lai ham show() khi can sinh them du lieu,
 * thay vi driver phai tu code logic phan trang thu cong.
 * single_open() la ham tien ich danh cho truong hop noi dung don gian,
 * sinh toan bo trong mot lan goi show() (khong can duyet qua nhieu "record").
 */
static struct proc_dir_entry *proc_entry;

static int lab2_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "LAB-02 Driver Statistics\n");
	seq_printf(m, "======================\n");
	seq_printf(m, "Driver name   : %s\n", DEVICE_NAME);
	seq_printf(m, "Major number  : %d\n", major_number);
	seq_printf(m, "Buffer size   : %d bytes\n", BUFFER_SIZE);
	seq_printf(m, "Data length   : %d bytes\n", buffer_len);
	seq_printf(m, "Open count    : %d\n", open_count);
	if (buffer_len > 0)
		seq_printf(m, "Last data     : [%s]\n", device_buffer);
	return 0;
}

static int lab2_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, lab2_proc_show, NULL);
}

static const struct proc_ops lab2_proc_fops = {
	.proc_open    = lab2_proc_open,
	.proc_read    = seq_read,
	.proc_lseek   = seq_lseek,
	.proc_release = single_release,
};

/* ===== SYSFS Implementation =====
 * sysfs export tung thuoc tinh rieng le thanh 1 file duoi kobject cua device.
 * Macro DEVICE_ATTR_RO(name) tu dong sinh ra struct device_attribute
 * dev_attr_name voi quyen chi doc (0444) va tro toi ham name_show() ben duoi
 * (quy uoc dat ten ham: <attr>_show). Co che nay dua tren kobject - moi
 * device trong kernel co mot kobject nhung vao cay /sys, va sysfs_create_group()
 * gan mot nhom attribute (file) vao kobject do.
 */
static ssize_t buffer_len_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", buffer_len);
}

static ssize_t open_count_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", open_count);
}

static ssize_t last_data_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	if (buffer_len > 0)
		return sprintf(buf, "%s\n", device_buffer);
	return sprintf(buf, "(empty)\n");
}

static DEVICE_ATTR_RO(buffer_len);
static DEVICE_ATTR_RO(open_count);
static DEVICE_ATTR_RO(last_data);

static struct attribute *lab2_attrs[] = {
	&dev_attr_buffer_len.attr,
	&dev_attr_open_count.attr,
	&dev_attr_last_data.attr,
	NULL,
};

static struct attribute_group lab2_attr_group = {
	.attrs = lab2_attrs,
};

/* ===== file_operations cua character device ===== */

/* Goi moi khi userspace open("/dev/lab2"). Chi dem so lan mo, khong cap phat
 * them tai nguyen vi driver dung mot bo dem toan cuc duy nhat.
 */
static int lab2_open(struct inode *inode, struct file *file)
{
	mutex_lock(&lab2_mutex);
	open_count++;
	mutex_unlock(&lab2_mutex);
	pr_info("lab2_driver: device opened (count=%d)\n", open_count);
	return 0;
}

/* Goi khi userspace close() fd. Khong co tai nguyen rieng per-fd nen chi log. */
static int lab2_release(struct inode *inode, struct file *file)
{
	pr_info("lab2_driver: device closed\n");
	return 0;
}

/*
 * lab2_read: chuyen du lieu tu device_buffer (kernel space) ra buf (user space).
 *
 * Khong dung memcpy() o day vi buf la mot con tro thuoc khong gian dia chi
 * CUA PROCESS USERSPACE, khac hoan toan voi khong gian dia chi kernel:
 *  - Con tro user co the khong hop le (process truyen bua con tro rac),
 *    chua duoc anh xa (page chua duoc load/swap in), hoac thuoc vung nho
 *    khong co quyen truy cap tu kernel.
 *  - memcpy() khong kiem tra tinh hop le cua dia chi dich/nguon, neu con
 *    tro user sai thi memcpy() se gay kernel oops/panic (truy cap dia chi
 *    khong map) hoac tao lo hong an ninh (doc/ghi nham vung nho kernel).
 * copy_to_user()/copy_from_user() kiem tra con tro co thuoc vung dia chi
 * userspace hop le khong (dung access_ok()), xu ly page fault mot cach an
 * toan (co the ngu/blocking de load trang nho), va tra ve ma loi (-EFAULT)
 * thay vi crash khi dia chi khong hop le.
 */
static ssize_t lab2_read(struct file *file, char __user *buf,
			  size_t len, loff_t *offset)
{
	int bytes_read = 0;

	if (*offset >= buffer_len)
		return 0;  /* EOF */
	if (len > buffer_len - *offset)
		len = buffer_len - *offset;

	if (copy_to_user(buf, device_buffer + *offset, len)) {
		pr_err("lab2_driver: copy_to_user failed\n");
		return -EFAULT;
	}

	*offset += len;
	bytes_read = len;
	pr_info("lab2_driver: sent %d bytes to user\n", bytes_read);
	return bytes_read;
}

/*
 * lab2_write: nhan du lieu tu buf (user space) ghi vao device_buffer (kernel).
 * Dung copy_from_user() voi cung ly do nhu tren (kiem tra dia chi nguon hop le).
 * mutex_lock() bao ve device_buffer/buffer_len khoi bi hai write() dong thoi
 * sua doi cung luc (xem giai thich mutex o phan khai bao bien toan cuc).
 */
static ssize_t lab2_write(struct file *file, const char __user *buf,
			   size_t len, loff_t *offset)
{
	if (len > BUFFER_SIZE - 1) {
		pr_warn("lab2_driver: write too large, truncating\n");
		len = BUFFER_SIZE - 1;
	}

	mutex_lock(&lab2_mutex);
	if (copy_from_user(device_buffer, buf, len)) {
		mutex_unlock(&lab2_mutex);
		return -EFAULT;
	}
	buffer_len = len;
	device_buffer[buffer_len] = '\0';
	mutex_unlock(&lab2_mutex);

	pr_info("lab2_driver: received %zu bytes: [%s]\n", len, device_buffer);
	return len;
}

static struct file_operations lab2_fops = {
	.owner   = THIS_MODULE,
	.open    = lab2_open,
	.release = lab2_release,
	.read    = lab2_read,
	.write   = lab2_write,
};

/*
 * lab2_init: ham goi khi insmod. Thu tu dang ky:
 *   1. register_chrdev() -> dang ky major number + file_operations voi kernel
 *   2. class_create() + device_create() -> tu dong sinh node /dev/lab2 va
 *      cay /sys/class/lab2_class/lab2/ (khong can goi mknod tay)
 *   3. proc_create() -> sinh /proc/lab2_info
 *   4. sysfs_create_group() -> sinh cac attribute file duoi /sys/.../lab2/
 * Neu buoc nao loi, phai rollback cac buoc truoc do de tranh leak resource.
 */
static int __init lab2_init(void)
{
	int ret;

	pr_info("lab2_driver: initializing module\n");

	ret = register_chrdev(major_number, DEVICE_NAME, &lab2_fops);
	if (ret < 0) {
		pr_err("lab2_driver: register_chrdev failed: %d\n", ret);
		return ret;
	}

	lab2_class = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(lab2_class)) {
		unregister_chrdev(major_number, DEVICE_NAME);
		return PTR_ERR(lab2_class);
	}

	lab2_device = device_create(lab2_class, NULL,
				     MKDEV(major_number, 0), NULL, DEVICE_NAME);
	if (IS_ERR(lab2_device)) {
		class_destroy(lab2_class);
		unregister_chrdev(major_number, DEVICE_NAME);
		return PTR_ERR(lab2_device);
	}

	/* Dang ky sysfs attribute group duoi kobject cua lab2_device */
	ret = sysfs_create_group(&lab2_device->kobj, &lab2_attr_group);
	if (ret)
		pr_warn("lab2_driver: sysfs_create_group failed\n");

	/* Tao /proc/lab2_info (quyen 0444: chi doc) */
	proc_entry = proc_create("lab2_info", 0444, NULL, &lab2_proc_fops);
	if (!proc_entry)
		pr_warn("lab2_driver: failed to create /proc/lab2_info\n");
	else
		pr_info("lab2_driver: /proc/lab2_info created\n");

	pr_info("lab2_driver: loaded, major=%d\n", major_number);
	return 0;
}

/*
 * lab2_exit: ham goi khi rmmod. Phai giai phong theo thu tu NGUOC voi init
 * de tranh truy cap vao resource da bi huy (vd: sysfs group gan voi kobject
 * cua device nen phai go truoc khi device_destroy()).
 */
static void __exit lab2_exit(void)
{
	if (proc_entry)
		proc_remove(proc_entry);

	sysfs_remove_group(&lab2_device->kobj, &lab2_attr_group);
	device_destroy(lab2_class, MKDEV(major_number, 0));
	class_destroy(lab2_class);
	unregister_chrdev(major_number, DEVICE_NAME);

	pr_info("lab2_driver: module unloaded\n");
}

module_init(lab2_init);
module_exit(lab2_exit);
