include $(srctree)/drivers/staging/imgtec/config_kernel.mk

obj-$(CONFIG_POWERVR_MT8173) += mt8173.o
mt8173-y += mt8173_mfgsys.o
mt8173-y += mt8173_sysconfig.o
mt8173-y += ../pvrsrvkm/$(PVR_ARCH)/dma_support.o
mt8173-y += ../pvrsrvkm/$(PVR_ARCH)/ion_support_generic.o
ccflags-y += -include $(srctree)/drivers/staging/imgtec/config_kernel.h \
 -include $(srctree)/include/generated/uapi/linux/version.h \
 -I$(srctree)/$(src) \
 -I$(srctree)/drivers/staging/imgtec \
 -I$(srctree)/drivers/staging/imgtec/pvrsrvkm \
 -I$(srctree)/drivers/staging/imgtec/pvrsrvkm/$(PVR_ARCH) \
 -I$(srctree)/drivers/staging/imgtec/pvrsrvkm/$(PVR_ARCH)/hwdefs \
 -I$(srctree)/drivers/staging/imgtec/pvrsrvkm/$(PVR_ARCH)/hwdefs/km

tmp := $(addprefix -I,$(wildcard $(srctree)/drivers/staging/imgtec/pvrsrvkm/$(PVR_ARCH)/generated/*))
ccflags-y += $(tmp)
