/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see the file COPYING, or write
 * to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
#ifndef VIOC_GLOBAL_H
#define	VIOC_GLOBAL_H


/* VIOC Overall Register Map.
 * ex)
 *  upper 2bits are component device [3:2]
 *  lower 2bits are component device number [1:0]
 *  0x0000 => RDMA[0]
 *  0x0001 => RDMA[1]
 *  0x0101 => WDMA[0]
 *  ...
 *  WARNING : DO NOT add 0x00XX type. it will be potential problem.
 */

#define get_vioc_type(x)		((x) >> 8U)
#define get_vioc_index(x)		((x) & 0xFFU)

/* DISP : 0x01XX */
#define VIOC_DISP			(0x0100U)
#define VIOC_DISP0			(0x0100U)
#define VIOC_DISP1			(0x0101U)
#define VIOC_DISP2			(0x0102U)
#define VIOC_DISP_MAX			(0x0003U)

/* RDMA : 0x02XX */
#define VIOC_RDMA			(0x0200U)
#define VIOC_RDMA00			(0x0200U)
#define VIOC_RDMA01			(0x0201U)
#define VIOC_RDMA02			(0x0202U)
#define VIOC_RDMA03			(0x0203U)
#define VIOC_RDMA04			(0x0204U)
#define VIOC_RDMA05			(0x0205U)
#define VIOC_RDMA06			(0x0206U)
#define VIOC_RDMA07			(0x0207U)
#define VIOC_RDMA08			(0x0208U)
#define VIOC_RDMA09			(0x0209U)
#define VIOC_RDMA10			(0x020AU)
#define VIOC_RDMA11			(0x020BU)
#define VIOC_RDMA12			(0x020CU)
#define VIOC_RDMA13			(0x020DU)
#define VIOC_RDMA14			(0x020EU)
#define VIOC_RDMA15			(0x020FU)
#define VIOC_RDMA16			(0x0210U)
#define VIOC_RDMA17			(0x0211U)
#define VIOC_RDMA_MAX		(0x0012U)

/* MC : 0x03XX */
#define VIOC_MC				(0x0300U)
#define VIOC_MC0			(0x0300U)
#define VIOC_MC1			(0x0301U)
#define VIOC_MC_MAX			(0x0002U)

/* WMIXER : 0x04XX */
#define VIOC_WMIX			(0x0400U)
#define VIOC_WMIX0			(0x0400U)
#define VIOC_WMIX1			(0x0401U)
#define VIOC_WMIX2			(0x0402U)
#define VIOC_WMIX3			(0x0403U)
#define VIOC_WMIX4			(0x0404U)
#define VIOC_WMIX5			(0x0405U)
#define VIOC_WMIX6			(0x0406U)
#define VIOC_WMIX_MAX			(0x0007U)

/* Scaler : 0x05XX */
#define VIOC_SCALER			(0x0500U)
#define VIOC_SCALER0			(0x0500U)
#define VIOC_SCALER1			(0x0501U)
#define VIOC_SCALER2			(0x0502U)
#define VIOC_SCALER3			(0x0503U)
#define VIOC_SCALER4			(0x0504U)
#define VIOC_SCALER5			(0x0505U)
#define VIOC_SCALER6			(0x0506U)
#define VIOC_SCALER7			(0x0507U)
#define VIOC_SCALER_MAX			(0x0008U)

/* DTRC : 0x06XX */
//#define VIOC_DTRC			(0x0600)

/* WDMA : 0x07XX */
#define VIOC_WDMA			(0x0700U)
#define VIOC_WDMA00			(0x0700U)
#define VIOC_WDMA01			(0x0701U)
#define VIOC_WDMA02			(0x0702U)
#define VIOC_WDMA03			(0x0703U)
#define VIOC_WDMA04			(0x0704U)
#define VIOC_WDMA05			(0x0705U)
#define VIOC_WDMA06			(0x0706U)
#define VIOC_WDMA07			(0x0707U)
#define VIOC_WDMA08			(0x0708U)
#define VIOC_WDMA09			(0x0709U)
#define VIOC_WDMA10			(0x070AU)
#define VIOC_WDMA11			(0x070BU)
#define VIOC_WDMA_MAX			(0x000CU)

/* DEINTL_S : 0x08XX */
#define VIOC_DEINTLS			(0x0800U)
#define VIOC_DEINTLS0			(0x0800U)
#define VIOC_DEINTLS_MAX		(0x0001U)

/* Async FIFO : 0x09XX */
#define VIOC_FIFO			(0x0900U)
#define VIOC_FIFO0			(0x0900U)
#define VIOC_FIFO_MAX			(0x0001U)

/* VIDEO IN : 0x0AXX */
#define VIOC_VIN			(0x0A00U)
#define VIOC_VIN00			(0x0A00U)
#define VIOC_VIN01			(0x0A01U)
#define VIOC_VIN10			(0x0A02U)
#define VIOC_VIN11			(0x0A03U)
#define VIOC_VIN20			(0x0A04U)
#define VIOC_VIN21			(0x0A05U)
#define VIOC_VIN30			(0x0A06U)
#define VIOC_VIN31			(0x0A07U)
#define VIOC_VIN40			(0x0A08U)
#define VIOC_VIN41			(0x0A09U)
#define VIOC_VIN50			(0x0A0AU)
#define VIOC_VIN51			(0x0A0BU)
#define VIOC_VIN60			(0x0A0CU)
#define VIOC_VIN61			(0x0A0DU)
#define VIOC_VIN_MAX			(0x000EU)

/* LUT DEV : 0x0BXX */
#define VIOC_LUT			(0x0B00U)
#define VIOC_LUT_DEV0			(0x0B00U)
#define VIOC_LUT_DEV1			(0x0B01U)
#define VIOC_LUT_DEV2			(0x0B02U)
#define VIOC_LUT_COMP0			(0x0B03U)
#define VIOC_LUT_COMP1			(0x0B04U)
#define VIOC_LUT_COMP2			(0x0B05U)
#define VIOC_LUT_COMP3			(0x0B06U)
#define VIOC_LUT_COMP_MAX			(0x0007U)
#define VIOC_LUT_MAX		(0x0007U)

/* LUT TABLE : 0x0CXX */
#define VIOC_LUT_TABLE			(0x0C00U)
#define VIOC_LUT_TABLE0			(0x0C00U)
#define VIOC_LUT_TABLE1			(0x0C01U)
#define VIOC_LUT_TABLE_MAX		(0x0002U)

/* VIOC CONFIG : 0x0DXX */
#define VIOC_CONFIG			(0x0D00U)

/* VIN DEMUX : 0x0EXX */
#define VIOC_VIN_DEMUX			(0x0E00U)

/* TIMER : 0x0FXX */
#define VIOC_TIMER			(0x0F00U)

/* VIQE : 0x10XX */
#define VIOC_VIQE			(0x1000U)
#define VIOC_VIQE0			(0x1000U)
#define VIOC_VIQE1			(0x1001U)
#define VIOC_VIQE_MAX			(0x0002U)

/* VIDEO MARK : 0x11XX */
//#define VIOC_VIDEOMARK		(0x1100)

/* NEXGUARD : 0x12XX */
//#define VIOC_NEXGUARD			(0x1200)

/* OUTCFG : 0x13XX */
#define VIOC_OUTCFG			(0x1300U)

/* V_EDR : 0x14XX */
//#define VIOC_V_EDR			(0x1400)

/* V_PANEL : 0x15XX */
//#define VIOC_V_PANEL			(0x1500)

/* V_PANEL_LUT : 0x16XX */
//#define VIOC_V_PANEL_LUT		(0x1600)

/* V_DV_CFG : 0x17XX */
//#define VIOC_V_DV_CFG			(0x1700)

/* AFBCDEC : 0x18XX */
#define VIOC_FBCDEC			(0x1800U)
#define VIOC_FBCDEC0			(0x1800U)
#define VIOC_FBCDEC1			(0x1801U)
#define VIOC_FBCDEC_MAX		(0x0002U)

/* VIOC Interrupt sources */
#define VIOC0_IRQI			(119U)
#define VIOC1_IRQI			(120U)
#define VIOC2_IRQI			(121U)
#define VIOC3_IRQI			(122U)
#define VIOC_IRQ_MAX		(0x4U)

/* Display Device : PXDW FORMAT */
#define VIOC_PXDW_FMT_04_STN			(0U)
#define VIOC_PXDW_FMT_08_STN			(1U)
#define VIOC_PXDW_FMT_08_RGB_STRIPE		(2U)
#define VIOC_PXDW_FMT_16_RGB565			(3U)
#define VIOC_PXDW_FMT_15_RGB555			(4U)
#define VIOC_PXDW_FMT_18_RGB666			(5U)
#define VIOC_PXDW_FMT_08_UY			(6U)
#define VIOC_PXDW_FMT_08_VY			(7U)
#define VIOC_PXDW_FMT_16_YU			(8U)
#define VIOC_PXDW_FMT_16_YV			(9U)
#define VIOC_PXDW_FMT_08_RGB_DELTA0		(10U)
#define VIOC_PXDW_FMT_08_RGB_DELTA1		(11U)
#define VIOC_PXDW_FMT_24_RGB888			(12U)
#define VIOC_PXDW_FMT_08_RGBD			(13U)
#define VIOC_PXDW_FMT_16_RGB666			(14U)
#define VIOC_PXDW_FMT_16_RGB888			(15U)
#define VIOC_PXDW_FMT_10_RGB_STRIPE_RGB		(16U)
#define VIOC_PXDW_FMT_10_RGB_DELTA_RGB_GBR	(17U)
#define VIOC_PXDW_FMT_10_RGB_DELTA_GBR_RGB	(18U)
#define VIOC_PXDW_FMT_10_YCBCR_CBYCRY		(19U)
#define VIOC_PXDW_FMT_10_YCBCR_CRYCBY		(20U)
#define VIOC_PXDW_FMT_20_YCBCR_YCBCR		(21U)
#define VIOC_PXDW_FMT_20_YCBCR_YCRCB		(22U)
#define VIOC_PXDW_FMT_30_RGB101010		(23U)
#define VIOC_PXDW_FMT_10_RGB_DUMMY		(24U)
#define VIOC_PXDW_FMT_20_RGB101010		(25U)
#define VIOC_PXDW_FMT_24_YCBCR420		(26U)
#define VIOC_PXDW_FMT_30_YCBCR420		(27U)

/* RDMA/WDMA : RGB Swap */
#define VIOC_SWAP_RGB				(0U)
#define VIOC_SWAP_RBG				(1U)
#define VIOC_SWAP_GRB				(2U)
#define VIOC_SWAP_GBR				(3U)
#define VIOC_SWAP_BRG				(4U)
#define VIOC_SWAP_BGR				(5U)

/* RDMA/WDMA : Image Format */
#define VIOC_IMG_FMT_BPP1		(0U)  // 1bit
#define VIOC_IMG_FMT_BPP2		(1U)  // 2bits
#define VIOC_IMG_FMT_BPP4		(2U)  // 4bits
#define VIOC_IMG_FMT_BPP8		(3U)  // 1byte
#define VIOC_IMG_FMT_RGB332		(8U)  // 1byte
#define VIOC_IMG_FMT_ARGB4444	(9U)  // 2bytes
#define VIOC_IMG_FMT_RGB565		(10U) // 2bytes
#define VIOC_IMG_FMT_ARGB1555	(11U) // 2bytes
#define VIOC_IMG_FMT_ARGB8888	(12U) // 4bytes
#define VIOC_IMG_FMT_ARGB6666_4	(13U) // 4bytes
#define	VIOC_IMG_FMT_RGB888		(14U) // 3bytes
#define VIOC_IMG_FMT_ARGB6666_3	(15U) // 3bytes
#define	VIOC_IMG_FMT_COMP		(16U) // 4bytes
#define	VIOC_IMG_FMT_DECOMP		(VIOC_IMG_FMT_COMP)
#define VIOC_IMG_FMT_444SEP		(21U) // 3bytes
#define	VIOC_IMG_FMT_UYVY		(22U) // 2bytes
#define	VIOC_IMG_FMT_VYUY		(23U) // 2bytes
#define VIOC_IMG_FMT_YUV420SEP	(24U) // 1,1byte
#define	VIOC_IMG_FMT_YUV422SEP	(25U) // 1,1byte
#define	VIOC_IMG_FMT_YUYV		(26U) // 2bytes
#define	VIOC_IMG_FMT_YVYU		(27U) // 2bytes
#define	VIOC_IMG_FMT_YUV420IL0	(28U) // 1,2byte
#define	VIOC_IMG_FMT_YUV420IL1	(29U) // 1,2byte
#define	VIOC_IMG_FMT_YUV422IL0	(30U) // 1,2bytes
#define	VIOC_IMG_FMT_YUV422IL1	(31U) // 1,2bytes

/* Configuration & Interrupt */
#define VIOC_EDR_WMIX0				(0U)
#define VIOC_EDR_EDR				(1U)

#define	VIOC_SC_RDMA_00				(0x00U)
#define	VIOC_SC_RDMA_01				(0x01U)
#define	VIOC_SC_RDMA_02				(0x02U)
#define	VIOC_SC_RDMA_03				(0x03U)
#define	VIOC_SC_RDMA_04				(0x04U)
#define	VIOC_SC_RDMA_05				(0x05U)
#define	VIOC_SC_RDMA_06				(0x06U)
#define	VIOC_SC_RDMA_07				(0x07U)
#define	VIOC_SC_RDMA_08				(0x08U)
#define	VIOC_SC_RDMA_09				(0x09U)
#define	VIOC_SC_RDMA_10				(0x0AU)
#define	VIOC_SC_RDMA_11				(0x0BU)
#define	VIOC_SC_RDMA_12				(0x0CU)
#define	VIOC_SC_RDMA_13				(0x0DU)
#define	VIOC_SC_RDMA_14				(0x0EU)
#define	VIOC_SC_RDMA_15				(0x0FU)
#define	VIOC_SC_VIN_00				(0x10U)
#define	VIOC_SC_RDMA_16				(0x11U)
#define	VIOC_SC_VIN_01				(0x12U)
#define	VIOC_SC_RDMA_17				(0x13U)
#define	VIOC_SC_WDMA_00				(0x14U)
#define	VIOC_SC_WDMA_01				(0x15U)
#define	VIOC_SC_WDMA_02				(0x16U)
#define	VIOC_SC_WDMA_03				(0x17U)
#define	VIOC_SC_WDMA_04				(0x18U)
#define	VIOC_SC_WDMA_05				(0x19U)
#define	VIOC_SC_WDMA_06				(0x1AU)
#define	VIOC_SC_WDMA_07				(0x1BU)
#define	VIOC_SC_WDMA_08				(0x1CU)

#define VIOC_VIQE_RDMA_00			(0x00U)
#define VIOC_VIQE_RDMA_01			(0x01U)
#define VIOC_VIQE_RDMA_02			(0x02U)
#define VIOC_VIQE_RDMA_03			(0x03U)
#define VIOC_VIQE_RDMA_06			(0x04U)
#define VIOC_VIQE_RDMA_07			(0x05U)
#define VIOC_VIQE_RDMA_10			(0x06U)
#define VIOC_VIQE_RDMA_11			(0x07U)
#define VIOC_VIQE_RDMA_12			(0x08U)
#define VIOC_VIQE_RDMA_14			(0x09U)
#define VIOC_VIQE_VIN_00			(0x0AU)
#define VIOC_VIQE_RDMA_16			(0x0BU)
#define VIOC_VIQE_VIN_01			(0x0CU)
#define VIOC_VIQE_RDMA_17			(0x0DU)

#define VIOC_DEINTLS_RDMA_00			(0x00U)
#define VIOC_DEINTLS_RDMA_01			(0x01U)
#define VIOC_DEINTLS_RDMA_02			(0x02U)
#define VIOC_DEINTLS_RDMA_03			(0x03U)
#define VIOC_DEINTLS_RDMA_06			(0x04U)
#define VIOC_DEINTLS_RDMA_07			(0x05U)
#define VIOC_DEINTLS_RDMA_10			(0x06U)
#define VIOC_DEINTLS_RDMA_11			(0x07U)
#define VIOC_DEINTLS_RDMA_12			(0x08U)
#define VIOC_DEINTLS_RDMA_14			(0x09U)
#define VIOC_DEINTLS_VIN_00			(0x0AU)
#define VIOC_DEINTLS_RDMA_16			(0x0BU)
#define VIOC_DEINTLS_VIN_01			(0x0CU)
#define VIOC_DEINTLS_RDMA_17			(0x0DU)

#define	VIOC_VM_RDMA_00				(0x00U)
#define	VIOC_VM_RDMA_01				(0x01U)
#define	VIOC_VM_RDMA_02				(0x02U)
#define	VIOC_VM_RDMA_03				(0x03U)
#define	VIOC_VM_RDMA_04				(0x04U)
#define	VIOC_VM_RDMA_05				(0x05U)
#define	VIOC_VM_RDMA_06				(0x06U)
#define	VIOC_VM_RDMA_07				(0x07U)
#define	VIOC_VM_RDMA_08				(0x08U)
#define	VIOC_VM_RDMA_09				(0x09U)
#define	VIOC_VM_RDMA_10				(0x0AU)
#define	VIOC_VM_RDMA_11				(0x0BU)
#define	VIOC_VM_RDMA_12				(0x0CU)
#define	VIOC_VM_RDMA_13				(0x0DU)
#define	VIOC_VM_RDMA_14				(0x0EU)
#define	VIOC_VM_RDMA_15				(0x0FU)
#define	VIOC_VM_VIN_00				(0x10U)
#define	VIOC_VM_RDMA_16				(0x11U)
#define	VIOC_VM_VIN_01				(0x12U)
#define	VIOC_VM_RDMA_17				(0x13U)
#define	VIOC_VM_WDMA_00				(0x14U)
#define	VIOC_VM_WDMA_01				(0x15U)
#define	VIOC_VM_WDMA_02				(0x16U)
#define	VIOC_VM_WDMA_03				(0x17U)
#define	VIOC_VM_WDMA_04				(0x18U)
#define	VIOC_VM_WDMA_05				(0x19U)
#define	VIOC_VM_WDMA_06				(0x1AU)
#define	VIOC_VM_WDMA_07				(0x1BU)
#define	VIOC_VM_WDMA_08				(0x1CU)

#define	VIOC_NG_RDMA_00				(0x00U)
#define	VIOC_NG_RDMA_01				(0x01U)
#define	VIOC_NG_RDMA_02				(0x02U)
#define	VIOC_NG_RDMA_03				(0x03U)
#define	VIOC_NG_RDMA_04				(0x04U)
#define	VIOC_NG_RDMA_05				(0x05U)
#define	VIOC_NG_RDMA_06				(0x06U)
#define	VIOC_NG_RDMA_07				(0x07U)
#define	VIOC_NG_RDMA_08				(0x08U)
#define	VIOC_NG_RDMA_09				(0x09U)
#define	VIOC_NG_RDMA_10				(0x0AU)
#define	VIOC_NG_RDMA_11				(0x0BU)
#define	VIOC_NG_RDMA_12				(0x0CU)
#define	VIOC_NG_RDMA_13				(0x0DU)
#define	VIOC_NG_RDMA_14				(0x0EU)
#define	VIOC_NG_RDMA_15				(0x0FU)
#define	VIOC_NG_VIN_00				(0x10U)
#define	VIOC_NG_RDMA_16				(0x11U)
#define	VIOC_NG_VIN_01				(0x12U)
#define	VIOC_NG_RDMA_17				(0x13U)
#define	VIOC_NG_WDMA_00				(0x14U)
#define	VIOC_NG_WDMA_01				(0x15U)
#define	VIOC_NG_WDMA_02				(0x16U)
#define	VIOC_NG_WDMA_03				(0x17U)
#define	VIOC_NG_WDMA_04				(0x18U)
#define	VIOC_NG_WDMA_05				(0x19U)
#define	VIOC_NG_WDMA_06				(0x1AU)
#define	VIOC_NG_WDMA_07				(0x1BU)
#define	VIOC_NG_WDMA_08				(0x1CU)

#define VIOC_RDMA_RDMA02			(0x02U)
#define VIOC_RDMA_RDMA03			(0x03U)
#define VIOC_RDMA_RDMA06			(0x06U)
#define VIOC_RDMA_RDMA07			(0x07U)
#define VIOC_RDMA_RDMA11			(0x0BU)
#define VIOC_RDMA_RDMA12			(0x0CU)
#define VIOC_RDMA_RDMA13			(0x0DU)
#define VIOC_RDMA_RDMA14			(0x0EU)
#define VIOC_RDMA_RDMA16			(0x10U)
#define VIOC_RDMA_RDMA17			(0x11U)

#define VIOC_MC_RDMA02				(0x02U)
#define VIOC_MC_RDMA03				(0x03U)
#define VIOC_MC_RDMA06				(0x06U)
#define VIOC_MC_RDMA07				(0x07U)
#define VIOC_MC_RDMA11				(0x0BU)
#define VIOC_MC_RDMA12				(0x0CU)
#define VIOC_MC_RDMA13				(0x0DU)
#define VIOC_MC_RDMA14				(0x0EU)
#define VIOC_MC_RDMA16				(0x10U)
#define VIOC_MC_RDMA17				(0x11U)

#define VIOC_DTRC_RDMA02			(0x02U)
#define VIOC_DTRC_RDMA03			(0x03U)
#define VIOC_DTRC_RDMA06			(0x06U)
#define VIOC_DTRC_RDMA07			(0x07U)
#define VIOC_DTRC_RDMA11			(0x0BU)
#define VIOC_DTRC_RDMA12			(0x0CU)
#define VIOC_DTRC_RDMA13			(0x0DU)
#define VIOC_DTRC_RDMA14			(0x0EU)
#define VIOC_DTRC_RDMA16			(0x10U)
#define VIOC_DTRC_RDMA17			(0x11U)

#define	VIOC_AD_RDMA_00				(0x00U)
#define	VIOC_AD_RDMA_01				(0x01U)
#define	VIOC_AD_RDMA_02				(0x02U)
#define	VIOC_AD_RDMA_03				(0x03U)
#define	VIOC_AD_RDMA_04				(0x04U)
#define	VIOC_AD_RDMA_05				(0x05U)
#define	VIOC_AD_RDMA_06				(0x06U)
#define	VIOC_AD_RDMA_07				(0x07U)
#define	VIOC_AD_RDMA_08				(0x08U)
#define	VIOC_AD_RDMA_09				(0x09U)
#define	VIOC_AD_RDMA_10				(0x0AU)
#define	VIOC_AD_RDMA_11				(0x0BU)
#define	VIOC_AD_RDMA_12				(0x0CU)
#define	VIOC_AD_RDMA_13				(0x0DU)
#define	VIOC_AD_RDMA_14				(0x0EU)
#define	VIOC_AD_RDMA_15				(0x0FU)
#define	VIOC_AD_RDMA_16				(0x11U)
#define	VIOC_AD_RDMA_17				(0x13U)

////////////////////////////////////////////////////////////////////////////////
//
//	CPUIF FORMAT
//
#define	CPUIF_OFMT_08_TYPE0   0U // LCDC : 8-8-8, SI(08bits) 8-8-8
#define	CPUIF_OFMT_24_TYPE0   4U // LCDC : 8-8-8, SI(16bits) 9-9
#define	CPUIF_OFMT_24_TYPE1   5U
#define	CPUIF_OFMT_24_TYPE2   6U
#define	CPUIF_OFMT_24_TYPE3   7U
#define	CPUIF_OFMT_18_TYPE0   8U
#define	CPUIF_OFMT_18_TYPE1   9U
#define	CPUIF_OFMT_18_TYPE2   10U
#define	CPUIF_OFMT_18_TYPE3   11U
#define	CPUIF_OFMT_16_TYPE0   12U
#define	CPUIF_OFMT_16_TYPE1   13U
#define	CPUIF_OFMT_16_TYPE2   14U
//
//	CPUIF FORMAT
//
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
//
//	WDMA SYNC SELECTION
//
#define	VIOC_WDMA_SYNC_ABSOLUTE		((u32)0U<<0U) // SYNCMD_ADDR
#define	VIOC_WDMA_SYNC_RELATIVE		((u32)1U<<0U) // SYNCMD_ADDR
#define	VIOC_WDMA_SYNC_START_EDGE	((u32)0U<<0U) // SYNCMD_SENS
#define	VIOC_WDMA_SYNC_START_LEVEL	((u32)1U<<0U) // SYNCMD_SENS

#define	VIOC_WDMA_SYNC_RDMA00		(0U)
#define	VIOC_WDMA_SYNC_RDMA01		(1U)
#define	VIOC_WDMA_SYNC_RDMA02		(2U)
#define	VIOC_WDMA_SYNC_RDMA03		(3U)
#define	VIOC_WDMA_SYNC_RDMA04		(4U)
#define	VIOC_WDMA_SYNC_RDMA05		(5U)
#define	VIOC_WDMA_SYNC_RDMA06		(6U)
#define	VIOC_WDMA_SYNC_RDMA07		(7U)
#define	VIOC_WDMA_SYNC_RDMA08		(8U)
#define	VIOC_WDMA_SYNC_RDMA09		(9U)
#define	VIOC_WDMA_SYNC_RDMA10		(10U)
#define	VIOC_WDMA_SYNC_RDMA11		(11U)
#define	VIOC_WDMA_SYNC_RDMA12		(12U)
#define	VIOC_WDMA_SYNC_RDMA13		(13U)
#define	VIOC_WDMA_SYNC_RDMA14		(14U)
#define	VIOC_WDMA_SYNC_RDMA15		(15U)
#define	VIOC_WDMA_SYNC_RDMA16		(16U)
#define	VIOC_WDMA_SYNC_RDMA17		(17U)
//
//	WDMA SYNC SELECTION
//
////////////////////////////////////////////////////////////////////////////////

/* VIOC DRIVER STATUS TYPE */
#define	VIOC_DEVICE_INVALID			(-2)
#define	VIOC_DEVICE_BUSY			(-1)
#define	VIOC_DEVICE_CONNECTED		(0)

/* VIOC DRIVER ERROR TYPE */
#define VIOC_DRIVER_ERR_INVALID		(-3)
#define VIOC_DRIVER_ERR_BUSY		(-2)
#define VIOC_DRIVER_ERR				(-1)
#define VIOC_DRIVER_NOERR			(0)

/* VIOC PATH STATUS TYPE */
#define VIOC_PATH_DISCONNECTED		(0U)
#define VIOC_PATH_CONNECTING		(1U)
#define VIOC_PATH_CONNECTED			(2U)
#define VIOC_PATH_DISCONNECTING		(3U)

#define FBX_MODE(x)			(x)
#define FBX_SINGLE			FBX_MODE(0x0U)
#define FBX_DOUBLE			FBX_MODE(0x1U)
#define FBX_TRIPLE			FBX_MODE(0x2U)

#define FBX_UPDATE(x)		(x)
#define FBX_RDMA_UPDATE			FBX_UPDATE(0x0U)
#define FBX_OVERLAY_UPDATE		FBX_UPDATE(0x1U)
#define FBX_NOWAIT_UPDATE		FBX_UPDATE(0x2U)

#define FBX_DEVICE(x)		(x)
#define FBX_DEVICE_NONE			FBX_DEVICE(0x0U)
#define FBX_DEVICE_LVDS			FBX_DEVICE(0x1U)
#define FBX_DEVICE_HDMI			FBX_DEVICE(0x2U)
#define FBX_DEVICE_COMPOSITE		FBX_DEVICE(0x3U)
#define FBX_DEVICE_COMPONENT		FBX_DEVICE(0x4U)

#endif
