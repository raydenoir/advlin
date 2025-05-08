obj-m     += int_stack.o # tell the kernel build to compile int_stack.c into int_stack.ko

KMOD      := int_stack
KSRC      := $(KMOD).c
USR       := kernel_stack
USRSRC    := $(USR).c

# kernel build infrastructure p.p
PWD       := $(shell pwd)
KDIR      := /lib/modules/$(shell uname -r)/build

.PHONY: all modules user clean

all: modules user # default target builds both kernel module and user utility

modules: # invoke the kernel’s Makefile on our folder
	$(MAKE) -C $(KDIR) M=$(PWD) modules 

user: # compile kernel_stack.c into the CLI
	gcc -Wall -o $(USR) $(USRSRC) 

clean: # clean up kernel build artifacts and remove user binary with leftovers
	$(MAKE) -C $(KDIR) M=$(PWD) clean 
	$(RM) $(USR) *.o *.ko .*.cmd Module.*

