# LAB-03 D.3 – ve histogram latency tu cyclictest
# Luu y: gnuplot KHONG mo rong '~' trong duong dan -> dung duong dan tuyet doi.
# Chay: gnuplot ~/embedded_lab3/logs/plot_latency.gnuplot
LOG = '/home/kien/embedded_lab3/logs/'

# ---- Hinh 1: Standard kernel + stress-ng, 4 thread ----
set terminal pngcairo size 1200,600 font 'Sans,11'
set output LOG.'latency_histogram.png'
set title 'Cyclictest Latency Histogram – Standard Kernel 7.0.0-34-generic (PREEMPT\_DYNAMIC lazy) + stress-ng'
set xlabel 'Latency (microseconds)'
set ylabel 'Number of occurrences (log scale)'
set xrange [0:140]
set yrange [0.8:20000]
set logscale y
set grid
set key top right
set style fill solid 0.5 noborder
set boxwidth 0.8 relative
plot LOG.'hist_standard.txt' using ($1-0.3):($2>0?$2:1/0):(0.25) with boxes title 'Thread 0' lc rgb '#2E75B6', \
     ''                      using ($1-0.1):($3>0?$3:1/0):(0.25) with boxes title 'Thread 1' lc rgb '#C55A11', \
     ''                      using ($1+0.1):($4>0?$4:1/0):(0.25) with boxes title 'Thread 2' lc rgb '#548235', \
     ''                      using ($1+0.3):($5>0?$5:1/0):(0.25) with boxes title 'Thread 3' lc rgb '#7030A0'

# ---- Hinh 2: so sanh khong tai vs co tai (tong 4 thread) ----
set output LOG.'latency_compare.png'
set title 'Cyclictest – Khong tai vs stress-ng (tong 4 thread, 40000 mau)'
set xrange [0:140]
plot LOG.'hist_idle.txt'     using ($1-0.2):(($2+$3+$4+$5)>0?($2+$3+$4+$5):1/0):(0.4) with boxes title 'Khong tai (idle)' lc rgb '#2E75B6', \
     LOG.'hist_standard.txt' using ($1+0.2):(($2+$3+$4+$5)>0?($2+$3+$4+$5):1/0):(0.4) with boxes title 'Co tai (stress-ng cpu+io+vm)' lc rgb '#C55A11'

# ---- Hinh 3: PREEMPT_DYNAMIC lazy (mac dinh) vs full (= kernel lowlatency), co tai ----
set output LOG.'latency_lazy_vs_full.png'
set title 'Cyclictest + stress-ng – preempt=lazy (mac dinh) vs preempt=full (lowlatency), tong 4 thread'
set xrange [0:140]
plot LOG.'hist_standard.txt'             using ($1-0.2):(($2+$3+$4+$5)>0?($2+$3+$4+$5):1/0):(0.4) with boxes title 'lazy  – Max 128 us' lc rgb '#C55A11', \
     LOG.'preempt_modes/hist_full.txt'   using ($1+0.2):(($2+$3+$4+$5)>0?($2+$3+$4+$5):1/0):(0.4) with boxes title 'full  – Max 40 us' lc rgb '#548235'
