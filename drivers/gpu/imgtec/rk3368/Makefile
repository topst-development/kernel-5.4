include $(srctree)/drivers/staging/imgtec/config_kernel.mk

obj-$(CONFIG_POWERVR_RK3368) += rk3368.o

rk3368-y += rk_init_v2.o \
	    sysconfig.o \
	    ../pvrsrvkm/$(PVR_ARCH)/dma_support.o \
	    ../pvrsrvkm/$(PVR_ARCH)/interrupt_support.o

ccflags-y += -include $(srctree)/drivers/staging/imgtec/config_kernel.h \
             -I$(srctree)/$(src) \
             -I$(srctree)/drivers/staging/imgtec \
             -I$(srctree)/drivers/staging/imgtec/pvrsrvkm \
             -I$(srctree)/drivers/staging/imgtec/pvrsrvkm/$(PVR_ARCH) \
             -I$(srctree)/drivers/staging/imgtec/pvrsrvkm/$(PVR_ARCH)/hwdefs \
             -I$(srctree)/drivers/staging/imgtec/pvrsrvkm/$(PVR_ARCH)/hwdefs/km

tmp := $(addprefix -I,$(wildcard $(srctree)/drivers/staging/imgtec/pvrsrvkm/$(PVR_ARCH)/generated/*))
ccflags-y += $(tmp)
