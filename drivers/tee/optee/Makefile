# SPDX-License-Identifier: GPL-2.0
obj-$(CONFIG_OPTEE) += optee.o
optee-objs += core.o
optee-objs += call.o
optee-objs += rpc.o
optee-objs += supp.o
optee-objs += shm_pool.o
optee-objs += device.o
ifeq ($(CONFIG_ARCH_TCC),y)
optee-objs += optee_version.o
endif
