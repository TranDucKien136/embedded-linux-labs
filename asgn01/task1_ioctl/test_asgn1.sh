#!/bin/sh
APP=/root/asgn1_app
DEV=/dev/asgn1
insmod /lib/modules/5.15.0/asgn1_driver.ko
[ -e $DEV ] || mknod $DEV c 244 0
$APP set_mode 1
echo "=== Echo mode ==="
echo "Hello World" > $DEV
cat $DEV
echo "=== Kernel mode ==="
$APP set_mode 0
echo "Hello World" > $DEV
cat $DEV
echo "=== ioctl ==="
$APP get_version
$APP get_stats
$APP set_mode 5
$APP bad_command
echo "=== Reset ==="
echo "data truoc reset" > $DEV
$APP reset_buffer
cat $DEV
$APP get_stats
echo "=== Unload ==="
rmmod asgn1_driver
dmesg | grep asgn1 | tail -n 20
