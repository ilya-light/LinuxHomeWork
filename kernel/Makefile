obj-m += myfs.o
myfs-y := main.o super.o inode.o dir.o file.o ioctl.o

ccflags-y += -I$(src)/../include -Wall

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
