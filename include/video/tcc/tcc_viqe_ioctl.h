/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-License-Identifier: GPL-2.0+ WITH Linux-syscall-note */
/*
 * Copyright (C) Telechips Inc.
 */
#ifndef TCC_VIQE_IOCTL_H__
#define TCC_VIQE_IOCTL_H__

#define VIQE_DEV_NAME                  "viqe"
#define VIQE_DEV_MAJOR                 230U
#define VIQE_DEV_MINOR                 0U

#define IOCTL_VIQE_INITIALIZE          0xAF0U
#define IOCTL_VIQE_EXCUTE              0xAF1U
#define IOCTL_VIQE_DEINITIALIZE        0xAF2U
#define IOCTL_VIQE_SETTING             0xAF3U

#define IOCTL_VIQE_INITIALIZE_KERNEL   0xBF0U
#define IOCTL_VIQE_EXCUTE_KERNEL       0xBF1U
#define IOCTL_VIQE_DEINITIALIZE_KERNEL 0xBF2U
#define IOCTL_VIQE_SETTING_KERNEL      0xBF3U

struct VIQE_DI_TYPE {
	unsigned int lcdCtrlNo;
	unsigned int scalerCh;
	unsigned int onTheFly;
	unsigned int use_sDeintls;
	unsigned int srcWidth;
	unsigned int srcHeight;
	unsigned int crop_top, crop_bottom, crop_left, crop_right;
	unsigned int deinterlaceM2M;
	unsigned int renderCnt;
	unsigned int OddFirst;
	unsigned int DI_use;
	unsigned int multi_hwr;

	unsigned int address[6];
	unsigned int dstAddr;

	// only for Testing!!
	unsigned int nRDMA;
	unsigned int use_Viqe0;
};

#endif /* TCC_VIQE_IOCTL_H__*/