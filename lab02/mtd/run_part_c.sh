#!/bin/bash
# LAB-02 Phan C - MTD subsystem & JFFS2 (chay tren HOST Ubuntu, can sudo)
# Cach dung: sudo ./run_part_c.sh
# Toan bo output se duoc ghi ra ~/embedded_lab2/logs/mtd_jffs2.txt

set -x  # in moi lenh truoc khi chay, de log ro rang cho bao cao

echo "=== C.2: Load nandsim va kiem tra MTD ==="
modprobe nandsim first_id_byte=0x20 second_id_byte=0x35
modprobe mtdblock
modprobe jffs2
cat /proc/mtd
ls /dev/mtdblock* 2>&1
ls /dev/mtd*

# Khong gia dinh cung mtd0: may host co the da co san MTD khac (vd: BIOS flash)
# nen nandsim co the bi day sang mtdN khac. Tu dong do dung thiet bi bang cach
# doc dong co ten "NAND simulator partition 0" trong /proc/mtd.
MTDNUM=$(grep "NAND simulator partition 0" /proc/mtd | sed -n 's/^mtd\([0-9]*\):.*/\1/p')
if [ -z "$MTDNUM" ]; then
    echo "ERROR: khong tim thay NAND simulator partition trong /proc/mtd"
    exit 1
fi
MTDDEV="/dev/mtd${MTDNUM}"
MTDBLOCKDEV="/dev/mtdblock${MTDNUM}"
echo "=== Phat hien nandsim tai: $MTDDEV ($MTDBLOCKDEV) ==="

echo "=== Thong tin MTD device ==="
mtdinfo "$MTDDEV"
cat /proc/mtd

echo "=== C.3: Xoa va format JFFS2 ==="
flash_erase "$MTDDEV" 0 0

mkdir -p /tmp/jffs2_data
echo 'Hello from JFFS2!' > /tmp/jffs2_data/hello.txt
echo 'Embedded Linux Lab2' > /tmp/jffs2_data/info.txt
mkdir -p /tmp/jffs2_data/logs
date > /tmp/jffs2_data/logs/boot.log

mkfs.jffs2 \
    --root=/tmp/jffs2_data \
    --eraseblock=0x4000 \
    --output=/tmp/rootfs.jffs2 \
    --pad \
    --no-cleanmarkers
ls -lh /tmp/rootfs.jffs2

echo "=== Ghi JFFS2 image vao MTD ==="
nandwrite -p "$MTDDEV" /tmp/rootfs.jffs2

echo "=== C.4: Mount va thao tac voi JFFS2 ==="
mkdir -p /mnt/jffs2
mount -t jffs2 "$MTDBLOCKDEV" /mnt/jffs2

ls -la /mnt/jffs2/
cat /mnt/jffs2/hello.txt
cat /mnt/jffs2/info.txt
cat /mnt/jffs2/logs/boot.log

echo "=== Ghi them du lieu (test write) ==="
sh -c 'echo "Written at $(date)" >> /mnt/jffs2/logs/boot.log'
sh -c 'echo "New runtime file" > /mnt/jffs2/runtime.txt'
cat /mnt/jffs2/logs/boot.log

echo "=== Kiem tra persistence: umount roi mount lai ==="
umount /mnt/jffs2
mount -t jffs2 "$MTDBLOCKDEV" /mnt/jffs2
echo "--- noi dung boot.log sau khi mount lai (phai con nguyen) ---"
cat /mnt/jffs2/logs/boot.log
echo "--- noi dung runtime.txt sau khi mount lai (phai con nguyen) ---"
cat /mnt/jffs2/runtime.txt
umount /mnt/jffs2

echo "=== C.5: Dump MTD raw data ==="
nanddump -o -l 512 -f /tmp/dump.bin "$MTDDEV"
hexdump -C /tmp/dump.bin | head -20

echo "=== Cleanup - go nandsim ==="
rmmod nandsim
ls /dev/mtd* 2>/dev/null || echo 'MTD devices removed'

set +x
echo "=== DONE: Phan C hoan tat ==="
