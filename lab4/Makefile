KMOD      := int_stack
KSRC      := $(KMOD).c
USR       := kernel_stack
USRSRC    := $(USR).c

# kernel build infrastructure p.p
PWD       := $(shell pwd)
KDIR      := /lib/modules/$(shell uname -r)/build

.PHONY: all modules user clean

all: modules user

modules:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

user:
	gcc -Wall -o $(USR) $(USRSRC)

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	$(RM) $(USR) *.o *.ko .*.cmd Module.*

