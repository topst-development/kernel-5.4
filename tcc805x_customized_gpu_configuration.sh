#!/bin/bash

# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) Telechips Inc.
config_file=.config
gpu_vz=0
platform=linux
architecture=64
linux_backend=_drm

for line in `cat $config_file`
do
        if [[ "$line" == "CONFIG_ARM=y" ]]; then
                architecture=32
	fi
        if [[ "$line" == "CONFIG_POWERVR_DC_FBDEV=y" ]]; then
                linux_backend=_fb
        fi
	if [[ "$line" == "CONFIG_POWERVR_VZ=y" ]]; then
		gpu_vz=1
	fi
	if [[ "$line" == "CONFIG_ANDROID=y" ]]; then
                platform=android
		linux_backend=
	fi
done

if [ $gpu_vz -eq 0 ]; then
	sed -i 's/#define PMAP_SIZE_PVR_VZ\t\t\t(0x4100000)/#define PMAP_SIZE_PVR_VZ 0/g' ./include/dt-bindings/pmap/tcc805x/pmap-tcc805x-graphic.h
fi

#If you want to change PVR_AUTOVZ_WDG_PERIOD_MS, use the following command
#sed -i 's/#define PVR_AUTOVZ_WDG_PERIOD_MS 3000/#define PVR_AUTOVZ_WDG_PERIOD_MS 300/g' ./drivers/gpu/imgtec/config_kernel_"$platform$architecture$linux_backend".h
