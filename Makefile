KVERSION = $(shell uname -r)
obj-m := deco.o
CFLAGS_deco.o := -O2
all:
	make -C /lib/modules/$(KVERSION)/build M=$(PWD) modules
clean:
	make -C /lib/modules/$(KVERSION)/build M=$(PWD) clean
