KDIR := $(HOME)/embedded_lab1/kernel/linux-5.15
#build for ARM
ARCH := arm
#TienToTrinhBienDich
CROSS_COMPILE := arm-linux-gnueabihf-
#XacDinhFileObjectCanBuild
obj-m := asgn1_driver.o
APP := asgn1_app
#TaoLenhMake
all:
	$(MAKE) -C $(KDIR) M=$(CURDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules
	$(CROSS_COMPILE)gcc -static -Wall -o $(APP) $(APP).c
clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) clean
	rm -f $(APP)
.PHONY: all clean
