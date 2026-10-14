#!/bin/bash
# Doi che do preemption qua tham so khoi dong (kernel lockdown chan debugfs khi bat Secure Boot).
#   sudo ./set_preempt_boot.sh full      -> them preempt=full vao GRUB (co hieu luc sau khi reboot)
#   sudo ./set_preempt_boot.sh restore   -> tra /etc/default/grub ve ban goc
set -e
if [ "$(id -u)" -ne 0 ]; then echo "Can chay bang sudo"; exit 1; fi
G=/etc/default/grub
BAK=/etc/default/grub.lab03.bak

case "$1" in
  none|voluntary|full|lazy)
    [ -f "$BAK" ] || cp -p "$G" "$BAK"          # chi backup ban goc 1 lan
    cp -p "$BAK" "$G"
    sed -i "s/^GRUB_CMDLINE_LINUX_DEFAULT=\"\(.*\)\"/GRUB_CMDLINE_LINUX_DEFAULT=\"\1 preempt=$1\"/" "$G"
    grep '^GRUB_CMDLINE_LINUX_DEFAULT' "$G"
    update-grub 2>&1 | tail -2
    echo ">>> Da dat preempt=$1. Khoi dong lai may, roi chay: sudo ~/embedded_lab3/run_preempt_modes.sh"
    ;;
  restore)
    [ -f "$BAK" ] || { echo "Khong co backup – GRUB chua bi sua"; exit 0; }
    cp -p "$BAK" "$G" && rm -f "$BAK"
    grep '^GRUB_CMDLINE_LINUX_DEFAULT' "$G"
    update-grub 2>&1 | tail -2
    echo ">>> Da khoi phuc GRUB goc (co hieu luc tu lan khoi dong sau)"
    ;;
  *) echo "Dung: sudo $0 {full|none|voluntary|lazy|restore}"; exit 1 ;;
esac
