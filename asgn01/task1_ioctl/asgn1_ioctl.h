#ifndef ASGN1_IOCTL_H
#define ASGN1_IOCTL_H

/*
 * asgn1_ioctl.h - (Tao file header cho cac cau truc du lieu va cac lenh ioctl)
 * Tac gia: Tran Duc Kien - SE183502
 */

#include <linux/ioctl.h>


#define AS01_MAGIC_NUMBER 'J'  /* tao magic number cho cac lenh ioctl */
#define AS01_BUFFER_VERSION_LEN 64  /* kich co cua buffer version */

/* cau truc du lieu thong ke cho device */
struct  as01_stats{
    int open_count;  /* so lan mo device */
    int write_count; /* so lan ghi vao device */
    int read_count;  /* so lan doc tu device */
    int buffer_len;  /* so byte co trong buffer */
    int last_write_size; /* kich co cua lan ghi cuoi cung */
};

#define AS01_MODE_KERNEL 0 /* che do kernel  */
#define AS01_MODE_ECHO 1 /* che do echo  */
#define AS01_RESET_BUFFER _IO(AS01_MAGIC_NUMBER, 0) /* lenh ioctl de reset buffer cua device */
#define AS01_GET_STATS _IOR(AS01_MAGIC_NUMBER, 1, struct as01_stats) /* lenh ioctl de lay thong tin thong ke */
#define AS01_SET_MODE _IOW(AS01_MAGIC_NUMBER, 2, int) /* lenh ioctl de thiet lap che do */
#define AS01_GET_VERSION _IOR(AS01_MAGIC_NUMBER, 3, char[AS01_BUFFER_VERSION_LEN]) /* lenh ioctl de lay version */

#define AS01_IOCTL_MAXNR 3 /* ma lenh ioctl toi da */


#endif /* ASGN1_IOCTL_H */
