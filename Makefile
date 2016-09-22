obj-m := themodule.o
KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all:
	gcc -g tester.c -o tester
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	rm tester
	$(MAKE) -C $(KDIR) M=$(PWD) clean
