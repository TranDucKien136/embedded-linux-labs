# LOS201 – Embedded Linux Systems

Bài thực hành (LAB-01 → LAB-03) và assignment cá nhân (ASSIGNMENT-01, ASSIGNMENT-02) môn
**Embedded Linux Systems** – Trần Đức Kiên (SE183502).

## Môi trường

- Host: Ubuntu 26.04 (x86_64)
- Target: ARMv7-A Cortex-A9
- Emulator: QEMU Versatile Express (`vexpress-a9`)
- Kernel: Linux 5.15 · U-Boot 2022.04 · BusyBox 1.35.0
- Toolchain: `arm-linux-gnueabihf-gcc` 15.2.0

## Nội dung

| Bài | Chủ đề | Thư mục | Báo cáo |
|---|---|---|---|
| [LAB-01](#lab-01) | Cấu hình kernel & boot Embedded Linux | [lab01/](lab01/) | [PDF](lab01/bao_cao/SE183502_Lab01_BaoCao.pdf) · [MD](lab01/bao_cao/SE183502_Lab01_BaoCao.md) |
| [LAB-02](#lab-02) | Device driver & quản lý file system | [lab02/](lab02/) | [PDF](lab02/bao_cao/SE183502_Lab02_BaoCao.pdf) · [MD](lab02/bao_cao/SE183502_Lab02_BaoCao.md) |
| [LAB-03](#lab-03) | Debug & phân tích hiệu năng | [lab03/](lab03/) | [PDF](lab03/bao_cao/SE183502_Lab03_BaoCao.pdf) · [MD](lab03/bao_cao/SE183502_Lab03_BaoCao.md) |
| [ASSIGNMENT-01](#assignment-01) | ioctl & misc device driver | [asgn01/](asgn01/) | [MD](asgn01/bao_cao/baocao.md) |
| [ASSIGNMENT-02](#assignment-02) | Sensor Monitoring System | [asgn02/](asgn02/) | [MD](asgn02/bao_cao/BaoCao.md) |

Các bài nối tiếp nhau: LAB-01 tạo kernel/rootfs → LAB-02 viết driver trên kernel đó → LAB-03 thêm công cụ debug
vào rootfs → ASSIGNMENT-01 mở rộng driver → ASSIGNMENT-02 kết hợp driver, ứng dụng đa luồng và công cụ của LAB-03.

---

<a id="lab-01"></a>
## LAB-01 – Kernel Configuration & Embedded Linux Boot

**Phạm vi bài:**

- Cấu hình và cross-compile Linux kernel 5.15 cho `vexpress-a9`.
- Build U-Boot, boot kernel bằng `bootz` với Device Tree Blob.
- Xây dựng BusyBox root filesystem (static), đóng gói initramfs, boot QEMU với `-smp 1` và `-smp 2`.

**File trong repo:**

- `lab01/configs/kernel.config`, `lab01/configs/busybox.config`
- `lab01/rootfs/initramfs/` (`init`, `etc/inittab`, `etc/init.d/rcS`)
- `lab01/logs/partA_build.txt`, `partA_build_KCFLAGS_loi.txt`, `partA_defconfig.txt`, `partA_verify.txt`
- `lab01/logs/partB_uboot.txt`, `partB_uboot_bootz.txt`, `partB_kernel_panic.txt`
- `lab01/logs/partC_rootfs.txt`, `partC_boot_smp1.txt`, `partC_boot_smp2.txt`

**Kết quả đã kiểm chứng:**

- Build kernel, DTB, U-Boot: PASS
- Boot qua U-Boot `bootz`: PASS
- Boot initramfs BusyBox, 1 và 2 CPU: PASS
- Ghi lại lỗi: kernel panic khi thiếu rootfs; build hỏng khi đặt `KCFLAGS=-mfpu=vfpv3 -mfloat-abi=hard`

Xem: [lab01/](lab01/)

---

<a id="lab-02"></a>
## LAB-02 – Device Driver & File System Management

**Phạm vi bài:**

- Kernel module lifecycle, character device `/dev/lab2` (major 240) với buffer 1024 byte bảo vệ bằng mutex.
- Giao tiếp userspace qua procfs (`/proc/lab2_info`, seq_file) và sysfs (`/sys/class/lab2_class/lab2/`).
- Tự nạp driver lúc boot bằng BusyBox `inittab`/`rcS`.
- MTD/JFFS2 trên host bằng `nandsim` + `mtd-utils`.

**File trong repo:**

- `lab02/driver/lab2_driver.c`, `lab02/driver/Makefile`
- `lab02/rootfs/initramfs/etc/inittab`, `etc/init.d/rcS`, `usr/bin/test_driver.sh`, `usr/bin/test_procfs.sh`
- `lab02/mtd/run_part_c.sh`
- `lab02/logs/boot_log.txt`, `driver_test.txt`, `procfs_test.txt`, `dmesg_lab2.txt`, `mtd_jffs2.txt`

**Kết quả đã kiểm chứng:**

- Build module ARMv7, `insmod`/`rmmod` không lỗi: PASS
- read/write `/dev/lab2`, procfs, sysfs: PASS
- Auto-load lúc boot: PASS
- Tạo, mount, ghi/đọc JFFS2 trên nandsim: PASS

Xem: [lab02/](lab02/)

---

<a id="lab-03"></a>
## LAB-03 – Debugging & Performance Analysis

**Phạm vi bài:**

- Debug remote bằng `gdbserver` trên QEMU + `gdb-multiarch` trên host.
- Tìm memory leak bằng Valgrind (trước/sau khi sửa).
- Phân tích system call bằng `strace`, CPU hotspot bằng `perf` + flame graph.
- Đo scheduling latency bằng `cyclictest` (không tải / có tải, preempt lazy vs full).

**File trong repo:**

- `lab03/apps/buggy_app.c`, `leaky_app.c`, `leaky_app_fixed.c`, `perf_target.c`, `Makefile`, `gdb_cmds.gdb`, `run_gdb.sh`
- `lab03/logs/gdb_session.txt`, `valgrind_report.txt`, `valgrind_fixed.txt`
- `lab03/logs/strace_summary.txt`, `strace_full.txt`, `perf_report.txt`, `perf_stat.txt`, `flamegraph.svg`
- `lab03/logs/cyclictest_*.txt`, `hist_*.txt`, `latency_histogram.png`, `latency_lazy_vs_full.png`, `preempt_modes/`
- `lab03/run_preempt_modes.sh`, `run_sudo_parts.sh`, `set_preempt_boot.sh`

**Kết quả đã kiểm chứng:**

- Debug remote `buggy_app` qua `gdbserver` (breakpoint, step, xem biến): PASS
- Valgrind: leak ở `leaky_app`, 0 leak ở `leaky_app_fixed`: PASS
- strace / perf / flame graph: PASS
- cyclictest + histogram latency: PASS

Xem: [lab03/](lab03/)

---

<a id="assignment-01"></a>
## ASSIGNMENT-01 – Advanced Device Driver (ioctl & misc device)

**Phạm vi bài:**

- Task 1: mở rộng character driver `asgn1_driver` (major 244, `/dev/asgn1`) với 2 chế độ Echo/Kernel và 4 lệnh ioctl
  magic `'J'`: `AS01_RESET_BUFFER`, `AS01_GET_STATS`, `AS01_SET_MODE`, `AS01_GET_VERSION`.
- Task 2: misc device `counter_driver` (`/dev/counter`, major 10) dùng `atomic_t`, thống kê qua `/proc/counter_info`.
- Task 3–4: báo cáo phân tích thiết kế và câu hỏi lý thuyết.

**File trong repo:**

- `asgn01/task1_ioctl/asgn1_driver.c`, `asgn1_ioctl.h`, `asgn1_app.c`, `Makefile`, `test_asgn1.sh`
- `asgn01/task2_misc/counter_driver.c`, `Makefile`, `test_counter.sh`
- `asgn01/logs/test_asgn1_output.txt`, `test_counter_output.txt`, `dmesg_full.txt`, `task1_basic_rw.txt`, `task2_modinfo.txt`
- `asgn01/ghi_chu/loi_gap_phai.md` – nhật ký lỗi gặp phải

**Kết quả đã kiểm chứng (QEMU):**

- Build 2 module ARMv7: PASS
- Echo mode ghi/đọc 12 byte, Kernel mode đọc 21 byte có tiền tố `[KERNEL] `: PASS
- `GET_VERSION`, `GET_STATS` (5 trường), `SET_MODE` từ chối giá trị sai (`EINVAL`): PASS
- Lệnh ioctl lạ trả `ENOTTY`: PASS
- `RESET_BUFFER` đưa buffer length / read count / write count về 0: PASS
- `/proc/counter_info` tại 3 thời điểm, `rmmod` sạch: PASS

**Build:**

```bash
make -C asgn01/task1_ioctl        # KDIR mặc định: ~/embedded_lab1/kernel/linux-5.15
make -C asgn01/task2_misc
```

Xem: [asgn01/](asgn01/)

---

<a id="assignment-02"></a>
## ASSIGNMENT-02 – Sensor Monitoring System

**Phạm vi bài:**

- Kernel driver `sms_sensor_driver.ko`: misc device `/dev/sms_sensor` trả dữ liệu CSV (fixed-point 0.1 °C),
  ioctl `SMS_SET_INTERVAL` (magic `'Y'`, 1–10000 ms), thống kê `/proc/sms_stats`.
- Ứng dụng `sms_app` 4 thread: Sensor → circular buffer (mutex + condition variable) → Processor →
  POSIX mq `/sms_alarm` → Alert; mq `/sms_log` → Logger ghi `/tmp/sms.log`.
- Valgrind, GDB, strace, perf; RT scheduling cho Sensor thread và đo jitter.

**File trong repo:**

- `asgn02/driver/sms_sensor_driver.c`, `sms_sensor.h`, `Makefile`
- `asgn02/app/sms_app.c`, `sms_common.h`, `circular_buffer.{c,h}`, `{sensor,processor,alert,logger}_thread.{c,h}`, `Makefile`, `fake_sensor.sh`
- `asgn02/logs/sms_sample_run.txt`, `sms.log`, `proc_sms_stats_{truoc,giua,sau}.txt`
- `asgn02/logs/valgrind_sms.txt`, `memory_footprint.txt`, `proc_maps_sms.txt`
- `asgn02/logs/gdb_session.txt`, `strace_sms.txt`, `perf_sms.txt`
- `asgn02/logs/jitter_report.txt`, `jitter_before_fix.txt`, `jitter_run_console.txt`
- `asgn02/ghi_chu/loi_gap_phai.md` – nhật ký 8 lỗi gặp phải và cách sửa

### Task 1 – Ứng dụng đa luồng

- Chạy 60 s, interval 500 ms: 120 sample tạo ra, 120 sample xử lý, 0 buffer overflow
- Alert: TEMP_HIGH 22, HUMID_HIGH 15, TEMP_SPIKE 79
- Logger ghi `/tmp/sms.log`, dừng sạch và in thông báo tổng kết: PASS

### Task 2 – Quản lý bộ nhớ & tích hợp driver

- `/proc/sms_stats`: `read_count` 0 → 120 sau 60 s (yêu cầu 120 ± 10): PASS
- Valgrind (bản x86 + cảm biến giả): 0 errors, 23 allocs / 23 frees, 0 byte còn lại khi thoát: PASS
- Memory footprint 3 thời điểm và `/proc/PID/maps`: có log

### Task 3 – Debug & profiling

- GDB (kịch bản B – alert logic): breakpoint `processor_thread.c:94`, `info threads` thấy đủ 5 thread, `set variable` giả lập nhiệt độ
- `strace -c`: nhiều nhất là `clock_nanosleep`, `mq_timedreceive`, `futex`, `clock_gettime64`, `mq_timedsend`
- `perf`: CPU hotspot

### Task 4 – Real-time scheduling

Sensor thread dùng `clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME)`, `mlockall`, so sánh SCHED_OTHER và
SCHED_FIFO (dưới tải nền), 1201 mẫu mỗi chế độ:

| Chỉ số | SCHED_OTHER | SCHED_FIFO |
|---|---:|---:|
| Priority | 0 | 50 |
| Số mẫu | 1201 | 1201 |
| Overrun | 0 | 0 |
| Min jitter (µs) | 219 | 231 |
| Avg jitter (µs) | 289.10 | 408.54 |
| Max jitter (µs) | 669 | 953 |
| P99 jitter (µs) | 470 | 798 |
| 100–500 µs | 99.83 % | 93.67 % |
| 500–1000 µs | 0.17 % | 6.33 % |

Trên QEMU, SCHED_FIFO không cho jitter thấp hơn SCHED_OTHER; phân tích nguyên nhân nằm trong báo cáo.

### Task 5 – Báo cáo & demo

- Báo cáo: [asgn02/bao_cao/BaoCao.md](asgn02/bao_cao/BaoCao.md)
- Video demo 3–5 phút: nộp qua LMS (không đưa lên repo)

**Build:**

```bash
make -C asgn02/driver             # KDIR: ~/embedded_as2/kernel/linux-5.15 (POSIX_MQUEUE, HIGH_RES_TIMERS, PREEMPT, HZ=1000)
make -C asgn02/app arm            # sms_app cho QEMU
make -C asgn02/app host           # sms_app_x86 cho Valgrind/perf
```

Xem: [asgn02/](asgn02/)

---

## Ghi chú

- Repo chỉ chứa mã nguồn, cấu hình, script, log và báo cáo. Không chứa mã nguồn kernel/U-Boot/BusyBox và file
  build ra (`zImage`, `*.ko`, `initramfs*.cpio.gz`, binary) – build lại bằng các `Makefile` ở trên.
- `asgn01/` và `asgn02/` là bản chụp của hai repo riêng giữ toàn bộ lịch sử làm bài từng bước (tag `taskN-done`):
  `embedded-linux-as1`, `embedded-linux-as2`.
- Chỉ kết quả có log trong repo mới được đánh dấu PASS.
- `scripts/check_repo.sh` (gọi tự động qua pre-push hook) kiểm tra file bắt buộc, file nhị phân, thông tin nhạy cảm
  và link README trước mỗi lần push:

```bash
ln -sf ../../scripts/pre-push .git/hooks/pre-push
./scripts/check_repo.sh
```

Boot QEMU:

```bash
qemu-system-arm -M vexpress-a9 -cpu cortex-a9 -m 512M -smp 2 -nographic \
  -kernel zImage -dtb vexpress-v2p-ca9.dtb -initrd initramfs.cpio.gz \
  -append 'console=ttyAMA0,115200 rdinit=/sbin/init mem=512M'
```
