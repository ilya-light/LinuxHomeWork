CC ?= gcc
CFLAGS ?= -Wall -Wextra -O2 -g
CPPFLAGS += -I../include

all: myfsctl

myfsctl: myfsctl.c ../include/myfs_ioctl.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ myfsctl.c

clean:
	rm -f myfsctl
