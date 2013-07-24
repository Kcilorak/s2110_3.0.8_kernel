#ifneq ($(KERNELRELEASE),)
#obj-m	:= ntckbd.o ntcaux.o nuvec.o

#else
#KDIR	:= /lib/modules/$(shell uname -r)/build
#PWD	:= $(shell pwd)

#default:
#	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules

#install:
#	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules_install
#endif
#obj-y += ntckbd.o
#obj-y += ntcaux.o
obj-$(CONFIG_EC) += nuvec.o
obj-$(CONFIG_EC_API_DEBUG) += nuvec_api_debug.o
ifeq ($(TARGET_SMT_BUILD),1)
obj-y += nuvec_smt_interface.o
endif
