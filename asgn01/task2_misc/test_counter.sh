#!/bin/sh

echo "=== Load ==="
insmod /lib/modules/5.15.0/counter_driver.ko
ls -l /dev/counter
lsmod

echo "=== Proc truoc khi write ==="
cat /proc/counter_info

echo "=== Read auto-increment ==="
cat /dev/counter
cat /dev/counter
cat /dev/counter

echo "=== Write 100 ==="
echo 100 > /dev/counter
cat /dev/counter

echo "=== Proc sau khi write ==="
cat /proc/counter_info

echo "=== Write sai (phai bao Invalid argument) ==="
echo -5 > /dev/counter
echo abc > /dev/counter

echo "=== Reset ==="
echo 0 > /dev/counter
cat /dev/counter

echo "=== Proc sau khi reset ==="
cat /proc/counter_info

echo "=== Unload ==="
rmmod counter_driver
dmesg | grep counter | tail -n 20
