obj-m += argon1.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

dt:
	dtc -I dts -O dtb -o argon1.dtbo argon1.dts

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
