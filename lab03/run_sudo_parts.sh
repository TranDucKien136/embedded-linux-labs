#!/bin/bash
# LAB-03 – cac buoc can quyen root: Phan C.3 (perf) va Phan D (cyclictest)
# Chay: sudo ~/embedded_lab3/run_sudo_parts.sh
set -e
if [ "$(id -u)" -ne 0 ]; then echo "Can chay bang sudo"; exit 1; fi

U=${SUDO_USER:-kien}
L=/home/$U/embedded_lab3
LOG=$L/logs
APP=$L/apps/perf_target
FG=/home/$U/FlameGraph
for t in perf cyclictest stress-ng; do
    command -v $t >/dev/null || { echo "Thieu $t – cai: sudo apt install -y rt-tests stress-ng"; exit 1; }
done
[ -x "$APP" ] || { echo "Chua build $APP (make -C $L/apps)"; exit 1; }
cd "$L/apps"

# perf (linux-perf 7.0.x) tren may nay bi Segmentation fault LUC THOAT sau khi
# da ghi xong perf.data -> khong de set -e dung script; kiem tra file bang perf report.
perf_ok() { perf report -i "$1" --stdio > /dev/null 2>&1; }
perf_rec() {   # perf_rec <output> <cac tuy chon...>
    local out=$1; shift
    for opts in "" "--no-buildid" "--no-buildid --call-graph dwarf"; do
        rm -f "$out"
        perf record "$@" $opts -o "$out" "$APP" > /dev/null 2>>"$LOG/perf_errors.txt" \
            || echo "  (perf record $* $opts thoat voi ma $? – kiem tra du lieu...)"
        if [ -s "$out" ] && perf_ok "$out"; then echo "  -> $out hop le"; return 0; fi
    done
    echo "  -> KHONG tao duoc $out hop le"; return 1
}

# perf_target ghi /tmp/perf_test.txt. Neu file do da ton tai va thuoc user khac,
# fs.protected_regular=2 cam ca root mo de ghi -> fopen() tra NULL -> segfault.
rm -f /tmp/perf_test.txt

echo "=== C.3 Buoc 1: perf record -g ==="
: > "$LOG/perf_errors.txt"
perf --version | tee "$LOG/perf_version.txt"
perf_rec "$LOG/perf.data" -g || true

echo "=== C.3 Buoc 2: perf report ==="
perf report -i "$LOG/perf.data" --stdio > "$LOG/perf_report.txt" 2>/dev/null || true
perf report -i "$LOG/perf.data" --stdio --no-children --sort symbol,dso 2>/dev/null \
    | grep -E '^ +[0-9]' | head -15 > "$LOG/perf_top_symbols.txt"
cat "$LOG/perf_top_symbols.txt"

echo "=== C.3 Buoc 3: perf stat ==="
perf stat "$APP" 2>&1 >/dev/null | tee "$LOG/perf_stat.txt" || true

echo "=== C.3 Buoc 4 (Bonus): Flame Graph ==="
if [ -x "$FG/flamegraph.pl" ]; then
    if perf_rec "$LOG/perf_flame.data" -F 999 -g; then
        perf script -i "$LOG/perf_flame.data" > "$LOG/perf_out.txt" 2>/dev/null || true
        "$FG/stackcollapse-perf.pl" "$LOG/perf_out.txt" > "$LOG/perf_folded.txt"
        "$FG/flamegraph.pl" --title "perf_target – LAB-03" "$LOG/perf_folded.txt" > "$LOG/flamegraph.svg" \
            && echo "flamegraph.svg OK"
    fi
fi
rm -f /tmp/perf_test.txt
chown -R "$U:$U" "$LOG"
if [ "$1" = "perf" ]; then echo "=== XONG phan perf (bo qua Phan D) ==="; exit 0; fi

echo "=== D.2 Buoc 1: thong tin kernel host ==="
{
    uname -a
    uname -r | grep -i preempt || echo 'Standard kernel (no RT patch)'
    grep -E 'PREEMPT' /boot/config-$(uname -r) | head -10
    echo "--- preempt mode dang chay (PREEMPT_DYNAMIC):"
    cat /sys/kernel/debug/sched/preempt 2>/dev/null || dmesg | grep -i 'dynamic preempt' | tail -1 || true
    echo "--- CPU:"; nproc; grep -m1 'model name' /proc/cpuinfo
} | tee "$LOG/kernel_info.txt"

CT="cyclictest --mlockall --threads=4 --priority=80 --interval=1000 --distance=500 --loops=10000 --histogram=200 --quiet"

echo "=== D.2 (tham chieu) cyclictest KHONG tai (~10s) ==="
$CT --histfile="$LOG/hist_idle.txt" > "$LOG/cyclictest_idle.txt"
grep -E '^# (Min|Avg|Max) Latencies' "$LOG/hist_idle.txt" || true

echo "=== D.2 Buoc 2+3: cyclictest + stress-ng cpu/io/vm (~15s) ==="
stress-ng --cpu "$(nproc)" --io 4 --vm 2 --vm-bytes 256M --timeout 60s > "$LOG/stress_ng.txt" 2>&1 &
SPID=$!
sleep 3
$CT --histfile="$LOG/hist_standard.txt" > "$LOG/cyclictest_standard.txt"
kill $SPID 2>/dev/null || true; wait $SPID 2>/dev/null || true
grep -E '^# (Min|Avg|Max) Latencies|Overflows' "$LOG/hist_standard.txt" || true

chown -R "$U:$U" "$LOG"
echo "=== XONG – bao lai cho Claude ==="
