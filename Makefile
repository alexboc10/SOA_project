KPATH :=/lib/modules/$(shell uname -r)/build

obj-m += TBDE.o
TBDE-objs += tag_service.o
TBDE-objs += ./syscalls/tag_get.o ./syscalls/tag_send.o ./syscalls/tag_receive.o ./syscalls/tag_ctl.o 
TBDE-objs += ./lib/vtpmo.o ./lib/sys_table_handler.o ./lib/TST_handler.o

all:
	make -C $(KPATH) M=$(PWD) modules
clean:
	make -C $(KPATH) M=$(PWD) clean
test:
	-sudo rmmod tag_service
	sudo dmesg -C
	sudo insmod tag_service.ko
	sudo dmesg

