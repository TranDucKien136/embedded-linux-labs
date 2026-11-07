KDIR := $(HOME)/embedded_lab1/kernel/linux-5.15
#build for ARM
ARCH := arm
#TienToTrinhBienDich
CROSS_COMPILE := arm-linux-gnueabihf-
#XacDinhFileObjectCanBuild
obj-m := counter_driver.o
#TaoLenhMake
all:
	$(MAKE) -C $(KDIR) M=$(CURDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules
clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) clean
.PHONY: all clean
