KERNEL_VERSION=`uname -r`
KERNEL_SOURCE=/usr/src/linux-${KERNEL_VERSION}
PWD=`pwd`
SYSMAP_FILE=/boot/System.map-${KERNEL_VERSION}

# Search address of sys_call_table in the map of the kernel
SYS_CALL_TABLE_ADDR=$(shell grep sys_call_table ${SYSMAP_FILE} | cut -d' ' -f1)
EXTRA_CFLAGS += -g -DSYS_CALL_TABLE_ADDR=0x${SYS_CALL_TABLE_ADDR}

obj-m := verfs.o

default:
	make -C ${KERNEL_SOURCE} SUBDIRS=${PWD} modules

install:
	install -D -m 644 verfs.ko /lib/modules/${KERNEL_VERSION}/mymodules/verfs.ko
	/sbin/depmod -a

clean:
	rm -rf *.o *.ko *.mod.* .tmp_versions .*.cmd
