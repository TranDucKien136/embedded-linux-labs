#!/bin/bash
# LAB-03 D.5 (thuc nghiem) – do latency o che do preemption HIEN TAI cua kernel.
# Secure Boot -> lockdown=integrity chan /sys/kernel/debug/sched/preempt, nen che do duoc
# doi bang tham so khoi dong (xem set_preempt_boot.sh). Kernel "lowlatency" Ubuntu = generic + preempt=full.
# Chay: sudo ~/embedded_lab3/run_preempt_modes.sh
set -e
if [ "$(id -u)" -ne 0 ]; then echo "Can chay bang sudo"; exit 1; fi

U=${SUDO_USER:-kien}
L=/home/$U/embedded_lab3
LOG=$L/logs/preempt_modes
mkdir -p "$LOG"
for t in cyclictest stress-ng; do command -v $t >/dev/null || { echo "Thieu $t"; exit 1; }; done

# Che do dang chay: tu dmesg "Dynamic Preempt: <mode>" (in khi boot)
MODE=$(dmesg | sed -n 's/.*Dynamic Preempt: \([a-z]*\).*/\1/p' | tail -1)
[ -n "$MODE" ] || MODE=$(sed -n 's/.*preempt=\([a-z]*\).*/\1/p' /proc/cmdline)
[ -n "$MODE" ] || { echo "Khong xac dinh duoc che do preemption"; exit 1; }
echo "=== Che do preemption dang chay: $MODE ==="
{ date; uname -a; echo "cmdline: $(cat /proc/cmdline)"; dmesg | grep 'Dynamic Preempt' | tail -1; } > "$LOG/info_$MODE.txt"

SPID=""
cleanup() { [ -n "$SPID" ] && kill "$SPID" 2>/dev/null || true; chown -R "$U:$U" "$LOG"; }
trap cleanup EXIT

# Cung dieu kien voi lan do chinh (hist_standard.txt): 4 thread, 1 ms, 10000 vong, stress-ng cpu+io+vm
stress-ng --cpu "$(nproc)" --io 4 --vm 2 --vm-bytes 256M --timeout 40s > "$LOG/stress_$MODE.txt" 2>&1 &
SPID=$!
sleep 3
cyclictest --mlockall --threads=4 --priority=80 --interval=1000 --loops=10000 \
    --histogram=200 --quiet --histfile="$LOG/hist_$MODE.txt" > "$LOG/cyclictest_$MODE.txt" 2>&1
kill "$SPID" 2>/dev/null || true; wait "$SPID" 2>/dev/null || true; SPID=""
grep -E '^# (Min|Avg|Max) Latencies|Overflows' "$LOG/hist_$MODE.txt" || true

if [ "$MODE" != lazy ] && [ -f /etc/default/grub.lab03.bak ]; then
    echo "=== Khoi phuc GRUB goc ==="
    "$L/set_preempt_boot.sh" restore
fi
echo "=== XONG ($MODE) – bao lai cho Claude ==="
