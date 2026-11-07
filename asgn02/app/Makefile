SRCS := sms_app.c sensor_thread.c processor_thread.c alert_thread.c logger_thread.c circular_buffer.c
LIBS := -lpthread -lrt -lm
#TienToTrinhBienDich
CROSS_COMPILE := arm-linux-gnueabihf-
#compiler
CC := gcc
ARM_CC := $(CROSS_COMPILE)gcc
CFLAGS := -Wall -Wextra -O2 -g
#TaoLenhMake
arm:
	$(ARM_CC) $(CFLAGS) -static -o sms_app $(SRCS) $(LIBS)
host:
	$(CC) $(CFLAGS) -o sms_app_x86 $(SRCS) $(LIBS)
clean:
	rm -f sms_app sms_app_x86
.PHONY: arm host clean
