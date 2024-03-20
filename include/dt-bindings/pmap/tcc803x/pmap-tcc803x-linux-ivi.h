/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (C) 2020 Telechips Inc.
 */
#ifndef DT_BINDINGS_PMAP_TCC803X_LINUX_IVI_H
#define DT_BINDINGS_PMAP_TCC803X_LINUX_IVI_H

#define PMAP_BASE 0x40800000
#include <dt-bindings/pmap/tcc803x/pmap-tcc803x-graphic.h>
#include <dt-bindings/pmap/tcc803x/pmap-tcc803x-linux-ivi-display.h>
#include <dt-bindings/pmap/tcc803x/pmap-tcc803x-video.h>

/*-----------------------------------------------------------
 * Default Reserved Memory
 *-----------------------------------------------------------
 */
#define	PMAP_SIZE_UMP_RESERVED		(0x0 \
						+ PMAP_SIZE_CAMERA_PREVIEW0 \
						+ PMAP_SIZE_CAMERA_PREVIEW1 \
						+ PMAP_SIZE_CAMERA_PREVIEW2 \
						+ PMAP_SIZE_CAMERA_PREVIEW3 \
						+ PMAP_SIZE_CAMERA_PREVIEW4 \
						+ PMAP_SIZE_CAMERA_PREVIEW5 \
						+ PMAP_SIZE_CAMERA_PREVIEW6 \
						)

/*-----------------------------------------------------------
 * Secure Area 1 (CPU X, VPU X, GPU W, VIOC R)
 *-----------------------------------------------------------
 */
#define	PMAP_SIZE_SECURE_AREA_1		(0x0 \
						+ PMAP_SIZE_FB_VIDEO \
						+ PMAP_SIZE_FB1_VIDEO \
						+ PMAP_SIZE_FB2_VIDEO \
						+ PMAP_SIZE_FB3_VIDEO \
						)

/*-----------------------------------------------------------
 * Secure Area 2 (CPU X, VPU X, GPU X, VIOC R/W)
 *-----------------------------------------------------------
 */
#define	PMAP_SIZE_SECURE_AREA_2		(0x0 \
						+ PMAP_SIZE_OVERLAY \
						+ PMAP_SIZE_OVERLAY1 \
						+ PMAP_SIZE_OVERLAY_ROT \
						+ PMAP_SIZE_VIQE0 \
						+ PMAP_SIZE_VIQE1 \
						+ PMAP_SIZE_V4L2_VOUT0 \
						+ PMAP_SIZE_V4L2_VOUT1 \
						+ PMAP_SIZE_OSD \
						+ PMAP_SIZE_DUAL_DISPLAY \
						+ PMAP_SIZE_FB_WMIXER \
						)

/*-----------------------------------------------------------
 * Secure Area 3 (CPU X, VPU R/W, GPU X, VIOC R)
 *-----------------------------------------------------------
 */
#define PMAP_SIZE_SECURE_AREA_3		(0x0 \
						+ PMAP_SIZE_SECURE_INBUF \
						+ PMAP_SIZE_VIDEO_EXT \
						+ PMAP_SIZE_VIDEO_EXT2 \
						+ PMAP_SIZE_VIDEO \
						)

/*-----------------------------------------------------------
 * Secure Area 4 (CPU X, VPU X, GPU R/W, VIOC R/W)
 *-----------------------------------------------------------
 */
#define	PMAP_SIZE_SECURE_AREA_4		(0x0 \
						+ PMAP_SIZE_UMP_RESERVED \
						)

/*-----------------------------------------------------------
 * Default Reserved Memory
 *-----------------------------------------------------------
 */
#define PMAP_SIZE_TOTAL_RESERVED	(0x0 \
						+ PMAP_SIZE_ENC_EXT \
						+ PMAP_SIZE_ENC_EXT2 \
						+ PMAP_SIZE_ENC_EXT3 \
						+ PMAP_SIZE_ENC_EXT4 \
						+ PMAP_SIZE_ENC_EXT5 \
						+ PMAP_SIZE_ENC_EXT6 \
						+ PMAP_SIZE_ENC_EXT7 \
						+ PMAP_SIZE_ENC_EXT8 \
						+ PMAP_SIZE_ENC_EXT9 \
						+ PMAP_SIZE_ENC_EXT10 \
						+ PMAP_SIZE_ENC_EXT11 \
						+ PMAP_SIZE_ENC_EXT12 \
						+ PMAP_SIZE_ENC_EXT13 \
						+ PMAP_SIZE_ENC_EXT14 \
						+ PMAP_SIZE_ENC_EXT15 \
						+ PMAP_SIZE_VIDEO_SW_EXTRA \
						)

#define	RESERVED_MEM_BASE		(0x40800000)
#define	RESERVED_MEM_SIZE		(0x40000000)

#define	CAMERA_PGL_BASE			(RESERVED_MEM_BASE)
#define	CAMERA_VIQE_BASE		(CAMERA_PGL_BASE + PMAP_SIZE_CAMERA_PGL)
#define	CAMERA_PREVIEW_BASE		(CAMERA_VIQE_BASE + PMAP_SIZE_CAMERA_VIQE)
#define	CAMERA_PREVIEW1_BASE		(CAMERA_PREVIEW_BASE + PMAP_SIZE_CAMERA_PREVIEW0)
#define	CAMERA_PREVIEW2_BASE		(CAMERA_PREVIEW1_BASE + PMAP_SIZE_CAMERA_PREVIEW1)
#define	CAMERA_PREVIEW3_BASE		(CAMERA_PREVIEW2_BASE + PMAP_SIZE_CAMERA_PREVIEW2)
#define	CAMERA_PREVIEW4_BASE		(CAMERA_PREVIEW3_BASE + PMAP_SIZE_CAMERA_PREVIEW3)
#define	CAMERA_PREVIEW5_BASE		(CAMERA_PREVIEW4_BASE + PMAP_SIZE_CAMERA_PREVIEW4)
#define	CAMERA_PREVIEW6_BASE		(CAMERA_PREVIEW5_BASE + PMAP_SIZE_CAMERA_PREVIEW5)

#define	SECURE_AREA_1_BASE		(CAMERA_PREVIEW6_BASE + PMAP_SIZE_CAMERA_PREVIEW6)
#define	SECURE_AREA_2_BASE		(SECURE_AREA_1_BASE + PMAP_SIZE_SECURE_AREA_1)
#define	SECURE_AREA_3_BASE		(SECURE_AREA_2_BASE + PMAP_SIZE_SECURE_AREA_2)
#define	SECURE_AREA_4_BASE		(SECURE_AREA_3_BASE + PMAP_SIZE_SECURE_AREA_3)

#define	RESERVED_HEAP_BASE		(SECURE_AREA_4_BASE + PMAP_SIZE_SECURE_AREA_4)
#define	HEAP_SIZE_LEFT_OVER		(RESERVED_MEM_BASE + RESERVED_MEM_SIZE \
						- RESERVED_HEAP_BASE)
#if (PMAP_SIZE_TOTAL_RESERVED > HEAP_SIZE_LEFT_OVER)
#define	RESERVED_HEAP_SIZE		(HEAP_SIZE_LEFT_OVER)
#else
#define	RESERVED_HEAP_SIZE		(PMAP_SIZE_TOTAL_RESERVED)
#endif

#endif	//DT_BINDINGS_PMAP_TCC803X_LINUX_IVI_H
