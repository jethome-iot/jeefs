# SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
# U-Boot's stdint.h is a shim: supply its normal integer types before JEEFS.
ccflags-y += -include $(srctree)/include/linux/types.h
ccflags-y += -I$(srctree)/lib/jeefs_demo/include -DJEEFS_CRC32_UBOOT
obj-y += command.o read_identity.o jeefs_header.o jeefs_devid.o jeefs_walk.o
