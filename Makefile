ifneq ($(KERNELRELEASE),)
obj-m := dmaer_master.o
dmaer_master-objs := dmaer.o vc_support.o

else
KDIR ?= /lib/modules/`uname -r`/build

default:
	$(MAKE) -C $(KDIR) M=$$PWD

endif

