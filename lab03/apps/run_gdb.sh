#!/bin/bash
# LAB-03 Phan A – tu dong: boot QEMU, chay gdbserver trong guest, ket noi gdb-multiarch tu host.
# Log: logs/gdb_session_raw.txt (GDB) va run_tmp/qemu_console.txt (console QEMU)
L=$HOME/embedded_lab3
W=$L/run_tmp
mkdir -p "$W"; rm -f "$W/con"; mkfifo "$W/con"

qemu-system-arm -M vexpress-a9 -cpu cortex-a9 -m 512M -smp 2 -nographic \
  -kernel ~/embedded_lab1/output/zImage \
  -dtb ~/embedded_lab1/output/vexpress-v2p-ca9.dtb \
  -initrd "$L/output/initramfs_lab3.cpio.gz" \
  -append 'console=ttyAMA0,115200 rdinit=/sbin/init mem=512M' \
  -net nic -net user,hostfwd=tcp::1234-:1234 \
  < "$W/con" > "$W/qemu_console.txt" 2>&1 &
QPID=$!
exec 3> "$W/con"

sleep 14; printf '\r' >&3; sleep 1
printf 'gdbserver :1234 /usr/bin/buggy_app\r' >&3; sleep 3

cd "$L/apps"
timeout 300 gdb-multiarch -q -batch -x gdb_cmds.gdb buggy_app > "$L/logs/gdb_session_raw.txt" 2>&1
echo "gdb exit=$?"

sleep 2; printf 'poweroff -f\r' >&3; sleep 3
exec 3>&-
kill $QPID 2>/dev/null; wait $QPID 2>/dev/null
rm -f "$W/con"
