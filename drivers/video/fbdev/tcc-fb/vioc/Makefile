ifneq (,$(filter y,$(CONFIG_ARCH_TCC805X)))
EXTRA_CFLAGS += -I$(srctree)/drivers/clk/tcc
endif

ifeq ($(CONFIG_TCC_VIOC_LOW_API),y)
obj-y += vioc_rdma.o vioc_wdma.o vioc_wmix.o
obj-y += vioc_vin.o
obj-y += vioc_scaler.o vioc_viqe.o vioc_lut.o
obj-y += vioc_intr.o vioc_fifo.o vioc_ddicfg.o vioc_deintls.o
obj-y += vioc_config.o vioc_disp.o vioc_outcfg.o
obj-$(CONFIG_ARCH_TCC897X) += vioc_lvds_t.o
obj-$(CONFIG_ARCH_TCC803X) += vioc_pxdemux.o vioc_lvds.o 
obj-$(CONFIG_ARCH_TCC805X) += vioc_pxdemux.o vioc_lvds.o vioc_lut_3d.o
obj-$(CONFIG_ARCH_TCC805X) += vioc_timer.o

obj-$(CONFIG_VIOC_MAP_DECOMP)	+= vioc_mc.o tca_map_converter.o
obj-$(CONFIG_VIOC_AFBCDEC) += vioc_afbcdec.o
obj-$(CONFIG_VIOC_PVRIC_FBDC) += vioc_pvric_fbdc.o
endif
