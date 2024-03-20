# SPDX-License-Identifier: GPL-2.0
# Makefile for the Linux video drivers.
# 5 Aug 1999, James Simmons, <mailto:jsimmons@users.sf.net>
# Rewritten to use lists instead of if-statements.

# Each configuration option enables a list of files.
ifeq (,$(filter y,$(CONFIG_DRM_TCC)))
obj-y                             += vioc/
endif
ifneq (,$(filter y,$(CONFIG_FB_VIOC) $(CONFIG_FB_NEW)))
#obj-y                             +=
obj-y 				  += panel/ fb.o
endif

obj-$(CONFIG_FB_TCC_OVERLAY)	+= tcc_overlay.o
obj-y							+= g2d/

obj-$(CONFIG_TCC_VIDEO_DISPLAY_DEINTERLACE_MODE) += viqe/
