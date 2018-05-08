obj-m := network1.o

KVERSION := $(shell uname -r)
KDIR := /lib/modules/$(KVERSION)/build
PWD := $(shell pwd)

default:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

install:
	$(MAKE) -C $(KDIR) M=$(PWD) modules_install

rmall:
	rmall '*.o' '*.ko' '*.mod.c' Mod* mod* '.[a-z]*' '.tmp_versions/*'
	rmdall .tmp_versions
