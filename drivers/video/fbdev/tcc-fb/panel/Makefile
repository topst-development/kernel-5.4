# SPDX-License-Identifier: GPL-2.0
ifneq (,$(filter y,$(CONFIG_FB_PANEL_LVDS_TCC)))
EXTRA_CFLAGS += -I$(srctree)/drivers/video/fbdev/tcc-fb/panel
obj-y += panel_helper.o
obj-y += panel-lvds-tcc.o
endif

