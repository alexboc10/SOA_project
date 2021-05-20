KPATH :=/lib/modules/$(shell uname -r)/build

obj-m += tag_service.o
tag_service-objs += ./lib/vtpmo.o sys_table_handler.o

all:
	make -C $(KPATH) M=$(PWD) modules
clean:
	make -C $(KPATH) M=$(PWD) clean
test:
	-sudo rmmod tag_service
	sudo dmesg -C
	sudo insmod tag_service.ko
	sudo dmesg

