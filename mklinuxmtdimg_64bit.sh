#!/bin/bash

# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) Telechips Inc.

set -e

COLOR_Y="\x1b[1;33m"
COLOR_R="\x1b[1;31m"
COLOR_E="\x1b[0m"

# Overwrite kbuild output path by ${O} if exists
if [ -n "${O}" ]; then
	KBUILD_OUTPUT=${O}
fi

# Set kbuild output path as '.' if not set
if [ -z "${KBUILD_OUTPUT}" ]; then
	KBUILD_OUTPUT=.
fi

MKBOOTIMG=./scripts/mkbootimg

DOTCFG=${KBUILD_OUTPUT}/.config

ARCH=$(cat ${DOTCFG} | grep "CONFIG_ARCH_TCC[89]" | grep "=y")
ARCH=${ARCH#CONFIG_ARCH_}
ARCH=${ARCH%=y}

KERNEL_IMG_PATH="${KBUILD_OUTPUT}/arch/arm64/boot/Image"
KERNEL_OFFSET=0x80000

case ${ARCH} in
	TCC805X)
		if [ -z `grep CONFIG_TCC805X_CA53Q=y ${DOTCFG}` ]; then
			KERNEL_BASE="0x20000000"
		else
			KERNEL_BASE="0x40000000"
		fi
		;;
	*)
		KERNEL_BASE="0x20000000"
		;;
esac

if [ -z `grep CONFIG_SERIAL_AMBA_PL011=y ${DOTCFG}` ]; then
	CONSOLE_NAME=ttyS0
else
	CONSOLE_NAME=ttyAMA0
fi

CMDLINE="console=${CONSOLE_NAME},115200n8 androidboot.console=${CONSOLE_NAME}"

if [ "${KBUILD_OUTPUT}" = "." ]; then
	IMG_OUTDIR=${KBUILD_OUTPUT}/BUILD_${ARCH}
	IMG_OUT=${ARCH}_boot.img
else
	IMG_OUTDIR=${KBUILD_OUTPUT}
	IMG_OUT=boot.img
fi

mkdir -p ${IMG_OUTDIR}

echo
echo -e "[${COLOR_Y}MAKE Parameter${COLOR_E}]"
echo -e "Architecture : ${ARCH}"
echo -e "Kernel Image : ${KERNEL_IMG_PATH}"
echo -e "Base Address : ${KERNEL_BASE}"
echo -e "Text Offset  : ${KERNEL_OFFSET}"
echo -e "Boot Cmdline : ${CMDLINE}"

${MKBOOTIMG} --kernel ${KERNEL_IMG_PATH} \
	     --base ${KERNEL_BASE} \
	     --kernel_offset ${KERNEL_OFFSET} \
	     --cmdline "${CMDLINE}" \
	     --output "${IMG_OUTDIR}/${IMG_OUT}"

echo
echo -e "[${COLOR_Y}MAKE${COLOR_E}] ${COLOR_R}${IMG_OUT}${COLOR_E}"
echo
echo "========================================================="
echo
