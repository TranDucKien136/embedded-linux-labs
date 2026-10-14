# Embedded Linux Systems – Labs

Bài thực hành môn **Embedded Linux Systems** – Trần Đức Kiên (SE183502).

Target: **ARMv7-A Cortex-A9** trên QEMU `vexpress-a9`, kernel Linux 5.15, U-Boot 2022.04, BusyBox 1.35.0,
toolchain `arm-linux-gnueabihf-gcc` 15.2.0, host Ubuntu 26.04.

| Lab | Nội dung | Báo cáo |
|---|---|---|
| [LAB-01](lab01/) | Cấu hình & build kernel, U-Boot (`bootz`), BusyBox rootfs, initramfs, boot QEMU | [PDF](lab01/bao_cao/SE183502_Lab01_BaoCao.pdf) · [MD](lab01/bao_cao/SE183502_Lab01_BaoCao.md) |
| [LAB-02](lab02/) | Character device driver (`/dev/lab2`), procfs/sysfs, MTD + JFFS2 (nandsim), auto-load qua BusyBox init | [PDF](lab02/bao_cao/SE183502_Lab02_BaoCao.pdf) · [MD](lab02/bao_cao/SE183502_Lab02_BaoCao.md) |
| [LAB-03](lab03/) | GDB remote debug (gdbserver), Valgrind, strace/perf + flame graph, cyclictest (preempt lazy vs full) | [PDF](lab03/bao_cao/SE183502_Lab03_BaoCao.pdf) · [MD](lab03/bao_cao/SE183502_Lab03_BaoCao.md) |

## Cấu trúc

```
lab01/  configs/ (kernel.config, busybox.config)  rootfs/  logs/  bao_cao/
lab02/  driver/ (lab2_driver.c, Makefile)  mtd/  rootfs/  logs/  bao_cao/
lab03/  apps/ (buggy/leaky/perf_target, Makefile, gdb script)  rootfs/  logs/  bao_cao/  *.sh
```

Repo chỉ chứa mã nguồn, cấu hình, script, log và báo cáo. **Không** chứa:

- Mã nguồn kernel/U-Boot/BusyBox – tải từ kernel.org, ftp.denx.de, busybox.net (phiên bản ở trên).
- File build ra (`zImage`, `u-boot`, `*.ko`, `initramfs*.cpio.gz`, binary BusyBox/ứng dụng, `perf.data`) –
  build lại bằng hướng dẫn trong báo cáo và các `Makefile`.
- Thư mục `rootfs/` chỉ giữ file cấu hình và script (`/init`, `etc/`, `usr/bin/*.sh`); BusyBox, thư viện ARM,
  `gdbserver`, `strace` và device node được thêm vào khi build rootfs.

## Build nhanh

```bash
export ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf-      # KHÔNG đặt KCFLAGS (làm hỏng build kernel)

# LAB-02 – driver (cần cây kernel 5.15 đã build ở LAB-01; Makefile dùng M=$(PWD) nên phải cd vào)
cd lab02/driver && make KDIR=~/embedded_lab1/kernel/linux-5.15 && cd -

# LAB-03 – ứng dụng ARM + host; rootfs/initramfs mặc định ở ~/embedded_lab3 (đổi bằng LAB=...)
make -C lab03/apps LAB=~/embedded_lab3 && make -C lab03/apps LAB=~/embedded_lab3 initramfs
```

Boot QEMU:

```bash
qemu-system-arm -M vexpress-a9 -cpu cortex-a9 -m 512M -smp 2 -nographic \
  -kernel zImage -dtb vexpress-v2p-ca9.dtb -initrd initramfs.cpio.gz \
  -append 'console=ttyAMA0,115200 rdinit=/sbin/init mem=512M'
```
