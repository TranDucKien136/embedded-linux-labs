#!/bin/sh
# Gia lap driver sms_sensor qua FIFO de chay Valgrind tren host

FIFO=/tmp/fake_sensor

rm -f "$FIFO"
mkfifo "$FIFO"

while true; do
    t=$(shuf -i 200-450 -n 1)
    h=$(shuf -i 400-900 -n 1)
    ms=$(( $(date +%s%N) / 1000000 ))
    echo "$ms,$((t / 10)).$((t % 10)),$((h / 10)).$((h % 10))"
    sleep 0.5
done > "$FIFO"