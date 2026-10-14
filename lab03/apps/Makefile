# LAB-03 – build tat ca ung dung + dong goi initramfs
#   make          : build ARM + host binaries, copy vao rootfs
#   make initramfs: dong goi output/initramfs_lab3.cpio.gz
#   make clean

CROSS   := arm-linux-gnueabihf-
ARMFLAGS:= -march=armv7-a -mfpu=neon-vfpv3 -mfloat-abi=hard
LAB     := $(HOME)/embedded_lab3
ROOTFS  := $(LAB)/rootfs/initramfs
INITRD  := $(LAB)/output/initramfs_lab3.cpio.gz

ARM_BINS  := buggy_app leaky_app_arm perf_target_arm
HOST_BINS := leaky_app leaky_app_fixed perf_target

all: $(ARM_BINS) $(HOST_BINS) install

# ---- ARM (chay trong QEMU) ----
buggy_app: buggy_app.c
	$(CROSS)gcc -g -O0 $(ARMFLAGS) -o $@ $<

leaky_app_arm: leaky_app.c
	$(CROSS)gcc -g -O0 $(ARMFLAGS) -DMEASURE -o $@ $<

perf_target_arm: perf_target.c
	$(CROSS)gcc -g -O1 $(ARMFLAGS) -o $@ $< -lm

# ---- Host x86_64 (Valgrind, strace, perf) ----
leaky_app: leaky_app.c
	gcc -g -O0 -o $@ $<

leaky_app_fixed: leaky_app_fixed.c
	gcc -g -O0 -o $@ $<

perf_target: perf_target.c
	gcc -g -O1 -o $@ $< -lm

install: $(ARM_BINS)
	cp $(ARM_BINS) $(ROOTFS)/usr/bin/

initramfs: install
	cd $(ROOTFS) && find . | cpio -H newc -o --owner=0:0 --quiet | gzip > $(INITRD)
	@ls -lh $(INITRD)

clean:
	rm -f $(ARM_BINS) $(HOST_BINS)

.PHONY: all install initramfs clean
