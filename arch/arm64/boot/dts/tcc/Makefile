# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) Telechips Inc.

ifeq ($(CONFIG_ANDROID),y)
dtb-$(CONFIG_ARCH_TCC805X) += \
	tcc8050-android-lpd4x322_sv0.1.dtb \
	tcc8053-android-lpd4x322_sv0.1.dtb \
	tcc8059-android-lpd4x321_sv0.1.dtb \
	tcc8050-android-lpd4x322_sv1.0.dtb \
	tcc8053-android-lpd4x322_sv1.0.dtb

dtb-$(CONFIG_ARCH_TCC803X) += \
	tcc803xpe-android-lpd4321_sv1.0.dtb

else # CONFIG_ANDROID

dtb-$(CONFIG_ARCH_TCC803X) += \
	tcc8030-linux-lpd4321_sv0.1.dtb \
	tcc803xpe-linux-lpd4321_sv1.0.dtb

ifeq ($(CONFIG_TCC805X_CA53Q),y)
dtb-$(CONFIG_ARCH_TCC805X) += \
	tcc8050-linux-subcore-ivi-tost_sv0.1 \
	tcc8050-linux-subcore_sv0.1.dtb \
	tcc8053-linux-subcore_sv0.1.dtb \
	tcc8059-linux-subcore_sv0.1.dtb \
	tcc8050-linux-subcore_sv1.0.dtb \
	tcc8053-linux-subcore_sv1.0.dtb

else # CONFIG_TCC805X_CA53Q

dtb-$(CONFIG_ARCH_TCC805X) += \
	tcc8050-linux-ivi-tost_sv0.1.dtb \
	tcc8050-linux-lpd4x322_sv0.1.dtb \
	tcc8053-linux-lpd4x322_sv0.1.dtb \
	tcc8059-linux-lpd4x321_sv0.1.dtb \
	tcc8050-linux-dvrs-lpd4x322_sv0.1.dtb \
	tcc8053-linux-dvrs-lpd4x322_sv0.1.dtb \
	tcc8059-linux-dvrs-lpd4x321_sv0.1.dtb \
	tcc8050-linux-lpd4x322_sv1.0.dtb \
	tcc8053-linux-lpd4x322_sv1.0.dtb \
	tcc8050-linux-dvrs-lpd4x322_sv1.0.dtb \
	tcc8053-linux-dvrs-lpd4x322_sv1.0.dtb
endif # CONFIG_TCC805X_CA53Q
endif # CONFIG_ANDROID

ifeq ($(CONFIG_OF_BUILD_OVERLAY),y)
dtb-y += \
	tcc8050-soc.dtb

dtbo-y += \
	overlay-tcc8050-android-lpd4x322_sv0.1.dtbo \
	overlay-tcc8053-android-lpd4x322_sv0.1.dtbo \
	overlay-tcc8059-android-lpd4x321_sv0.1.dtbo \
	overlay-tcc8050-android-lpd4x322_sv1.0.dtbo \
	overlay-tcc8053-android-lpd4x322_sv1.0.dtbo
endif

always         := $(dtb-y) $(dtbo-y)
subdir-y       := $(dts-dirs)
clean-files    := *.dtb *.dtbo
