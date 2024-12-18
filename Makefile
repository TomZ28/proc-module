KDIR ?= /lib/modules/$(shell uname -r)/build

obj-m := proc_module.o

all:
	make -C $(KDIR) M=$(shell pwd) modules

clean:
	make -C $(KDIR) M=$(shell pwd) clean
