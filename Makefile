ifneq ($(KERNELRELEASE),)
	obj-m := mymodule.o
else
	KDIR    := /lib/modules/$(shell uname -r)/build
	PWD     := $(shell pwd)
all:
	make -C $(KDIR) M=$(PWD) modules
	gcc user.c -o user
clean:
	make -C $(KDIR) M=$(PWD) clean
	rm -rf user
endif

