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
#ifndef VIOC_AFBCDEC_H
#define	VIOC_AFBCDEC_H

typedef enum {
	VIOC_AFBCDEC_SWAP_DIRECT = 0,
	VIOC_AFBCDEC_SWAP_PENDING,
} VIOC_AFBCDEC_SWAP;

typedef enum {
	VIOC_AFBCDEC_MODE_CONTINUOUS = 0,
	VIOC_AFBCDEC_MODE_FRAMEBYFRAME,
} VIOC_AFBCDEC_MODE;

typedef enum {
	VIOC_AFBCDEC_SURFACE_0 = 0,
	VIOC_AFBCDEC_SURFACE_1,
	VIOC_AFBCDEC_SURFACE_2,
	VIOC_AFBCDEC_SURFACE_3,
	VIOC_AFBCODE_SURFACE_MAX
} VIOC_AFBCDEC_SURFACE_NUM;

/*
 * Register offset
 */
#define AFBCDEC_BLOCK_ID					(0x00U)
#define AFBCDEC_IRQ_RAW_STATUS				(0x04U)
#define AFBCDEC_IRQ_CLEAR					(0x08U)
#define AFBCDEC_IRQ_MASK					(0x0CU)
#define AFBCDEC_IRQ_STATUS					(0x10U)
#define AFBCDEC_COMMAND						(0x14U)
#define AFBCDEC_STATUS						(0x18U)
#define AFBCDEC_SURFACE_CFG					(0x1CU)
#define AFBCDEC_AXI_CFG						(0x20U)

#define AFBCDEC_S0_BASE						(0x40U)
#define AFBCDEC_S1_BASE						(0x70U)
#define AFBCDEC_S2_BASE						(0xA0U)
#define AFBCDEC_S3_BASE						(0xD0U)

#define AFBCDEC_S_HEADER_BUF_ADDR_LOW		(0x00U)
#define AFBCDEC_S_HEADER_BUF_ADDR_HIGH		(0x04U)
#define AFBCDEC_S_FORMAT_SPECIFIER			(0x08U)
#define AFBCDEC_S_BUFFER_SIZE				(0x0CU)
#define AFBCDEC_S_BOUNDING_BOX_X			(0x10U)
#define AFBCDEC_S_BOUNDING_BOX_Y			(0x14U)
#define AFBCDEC_S_OUTPUT_BUF_ADDR_LOW		(0x18U)
#define AFBCDEC_S_OUTPUT_BUF_ADDR_HIGH		(0x1CU)
#define AFBCDEC_S_OUTPUT_BUF_STRIDE			(0x20U)
#define AFBCDEC_S_PREFETCH_CFG				(0x24U)

/*
 * BLOCK_ID register Information
 */
#define AFBCDEC_PRODUCT_ID_SHIFT			(16U)
#define AFBCDEC_VERSION_MAJOR_SHIFT			(12U)
#define AFBCDEC_VERSION_MINOR_SHIFT			(4U)
#define AFBCDEC_VERSION_STATUS_SHIFT		(0U)

#define AFBCDEC_PRODUCT_ID_MASK		((u32)0xFFFFU << AFBCDEC_PRODUCT_ID_SHIFT)
#define AFBCDEC_VERSION_MAJOR_MASK	((u32)0xFU << AFBCDEC_VERSION_MAJOR_SHIFT)
#define AFBCDEC_VERSION_MINOR_MASK	((u32)0xFFU << AFBCDEC_VERSION_MINOR_SHIFT)
#define AFBCDEC_VERSION_STATUS_MASK	((u32)0xFU << AFBCDEC_VERSION_STATUS_SHIFT)

/*
 * IRQ_RAW_STATUS, IRQ_CLEAR, IRQ_MASK, IRQ_STATUS register Information
 */
#define AFBCDEC_IRQ_DETILING_ERR_SHIFT		(3U)
#define AFBCDEC_IRQ_DECODE_ERR_SHIFT		(2U)
#define AFBCDEC_IRQ_CONFIG_SWAPPED_SHIFT	(1U)
#define AFBCDEC_IRQ_SURF_COMPLETED_SHIFT	(0U)

#define AFBCDEC_IRQ_DETILING_ERR_MASK \
	((u32)0x1U << AFBCDEC_IRQ_DETILING_ERR_SHIFT)
#define AFBCDEC_IRQ_DECODE_ERR_MASK \
	((u32)0x1U << AFBCDEC_IRQ_DECODE_ERR_SHIFT)
#define AFBCDEC_IRQ_CONFIG_SWAPPED_MASK \
	((u32)0x1U << AFBCDEC_IRQ_CONFIG_SWAPPED_SHIFT)
#define AFBCDEC_IRQ_SURF_COMPLETED_MASK \
	((u32)0x1U << AFBCDEC_IRQ_SURF_COMPLETED_SHIFT)

#define AFBCDEC_IRQ_ALL \
	(AFBCDEC_IRQ_DETILING_ERR_MASK \
	|AFBCDEC_IRQ_DECODE_ERR_MASK \
	|AFBCDEC_IRQ_CONFIG_SWAPPED_MASK \
	|AFBCDEC_IRQ_SURF_COMPLETED_MASK)

/*
 * COMMAND register Information
 */
#define AFBCDEC_CMD_PENDING_SWAP_SHIFT		(1U)
#define AFBCDEC_CMD_DIRECT_SWAP_SHIFT		(0U)

#define AFBCDEC_CMD_PENDING_SWAP_MASK	((u32)0x1U << AFBCDEC_CMD_PENDING_SWAP_SHIFT)
#define AFBCDEC_CMD_DIRECT_SWAP_MASK	((u32)0x1U << AFBCDEC_CMD_DIRECT_SWAP_SHIFT)

/*
 * STATUS register Information
 */
#define AFBCDEC_STS_ERR_SHIFT				(2U)
#define AFBCDEC_STS_SWAPPING_SHIFT		(1U)
#define AFBCDEC_STS_ACTIVE_SHIFT			(0U)

#define AFBCDEC_STS_ERR_MASK		((u32)0x1U << AFBCDEC_STS_ERR_SHIFT)
#define AFBCDEC_STS_SWAPPING_MASK	((u32)0x1U << AFBCDEC_STS_SWAPPING_SHIFT)
#define AFBCDEC_STS_ACTIVE_MASK		((u32)0x1U << AFBCDEC_STS_ACTIVE_SHIFT)

/*
 * SURFACE_CFG register Information
 */
#define AFBCDEC_SURCFG_CONTI_SHIFT			(16U)
#define AFBCDEC_SURCFG_S3_SHIFT				(3U)
#define AFBCDEC_SURCFG_S2_SHIFT				(2U)
#define AFBCDEC_SURCFG_S1_SHIFT				(1U)
#define AFBCDEC_SURCFG_S0_SHIFT				(0U)

#define AFBCDEC_SURCFG_CONTI_MASK	((u32)0x1U << AFBCDEC_SURCFG_CONTI_SHIFT)
#define AFBCDEC_SURCFG_S3_MASK		((u32)0x1U << AFBCDEC_SURCFG_S3_SHIFT)
#define AFBCDEC_SURCFG_S2_MASK		((u32)0x1U << AFBCDEC_SURCFG_S2_SHIFT)
#define AFBCDEC_SURCFG_S1_MASK		((u32)0x1U << AFBCDEC_SURCFG_S1_SHIFT)
#define AFBCDEC_SURCFG_S0_MASK		((u32)0x1U << AFBCDEC_SURCFG_S0_SHIFT)

/*
 * AXI_CFG register Information
 */
#define AFBCDEC_AXICFG_CACHE_SHIFT	(4U)
#define AFBCDEC_AXICFG_QOS_SHIFT	(0U)

#define AFBCDEC_AXICFG_CACHE_MASK	((u32)0xFU << AFBCDEC_AXICFG_CACHE_SHIFT)
#define AFBCDEC_AXICFG_QOS_MASK		((u32)0xFU << AFBCDEC_AXICFG_QOS_SHIFT)

/*
 * Header Buffer Register Information
 */
#define AFBCDEC_HEADERBUF_ADDR_LOW_SHIFT		(6U)
#define AFBCDEC_HEADERBUF_ADDR_HIGH_SHIFT		(0U)

#define AFBCDEC_HEADERBUF_ADDR_LOW_MASK \
			((u32)0x3FFFFFFU << AFBCDEC_HEADERBUF_ADDR_LOW_SHIFT)
#define AFBCDEC_HEADERBUF_ADDR_HIGH_MASK \
			((u32)0xFFFFU << AFBCDEC_HEADERBUF_ADDR_HIGH_SHIFT)

/*
 * Mode & Format Register Information
 */
#define AFBCDEC_MODE_SUPERBLK_SHIFT		(16U)
#define AFBCDEC_MODE_SPLIT_SHIFT		(9U)
#define AFBCDEC_MODE_YUV_TRANSF_SHIFT	(8U)
#define AFBCDEC_MODE_PIXELFMT_SHIFT		(0U)

#define AFBCDEC_MODE_SUPERBLK_MASK	((u32)0x1U << AFBCDEC_MODE_SUPERBLK_SHIFT)
#define AFBCDEC_MODE_SPLIT_MASK		((u32)0x1U << AFBCDEC_MODE_SPLIT_SHIFT)
#define AFBCDEC_MODE_YUV_TRANSF_MASK ((u32)0x1U << AFBCDEC_MODE_YUV_TRANSF_SHIFT)
#define AFBCDEC_MODE_PIXELFMT_MASK	((u32)0xFU << AFBCDEC_MODE_PIXELFMT_SHIFT)

#define AFBCDEC_FORMAT_RGB565			(0U)
#define AFBCDEC_FORMAT_RGBA5551			(1U)
#define AFBCDEC_FORMAT_RGBA1010102		(2U)
#define AFBCDEC_FORMAT_10BIT_YUV420		(3U)
#define AFBCDEC_FORMAT_RGB888			(4U)
#define AFBCDEC_FORMAT_RGBA8888			(5U)
#define AFBCDEC_FORMAT_RGBA4444			(6U)
#define AFBCDEC_FORMAT_R8				(7U)
#define AFBCDEC_FORMAT_RG88				(8U)
#define AFBCDEC_FORMAT_8BIT_YUV420		(9U)
#define AFBCDEC_FORMAT_8BIT_YUV422		(11U)
#define AFBCDEC_FORMAT_10BIT_YUV422		(14U)

/*
 * Image Size Register Information
 */
#define AFBCDEC_SIZE_HEIGHT_SHIFT	(16U)
#define AFBCDEC_SIZE_WIDTH_SHIFT	(0U)

#define AFBCDEC_SIZE_HEIGHT_MASK	((u32)0x1FFFU << AFBCDEC_SIZE_HEIGHT_SHIFT)
#define AFBCDEC_SIZE_WIDTH_MASK		((u32)0x1FFFU << AFBCDEC_SIZE_WIDTH_SHIFT)

/*
 * Bounding box Register Information
 */
#define AFBCDEC_BOUNDING_BOX_MAX_SHIFT	(16U)
#define AFBCDEC_BOUNDING_BOX_MIN_SHIFT	(0U)

#define AFBCDEC_BOUNDING_BOX_MAX_MASK ((u32)0x1FFFU << AFBCDEC_BOUNDING_BOX_MAX_SHIFT)
#define AFBCDEC_BOUNDING_BOX_MIN_MASK ((u32)0x1FFFU << AFBCDEC_BOUNDING_BOX_MIN_SHIFT)

/*
 * Output Buffer Register Information
 */
#define AFBCDEC_OUTBUF_ADDR_LOW_SHIFT		(7U)
#define AFBCDEC_OUTBUF_ADDR_HIGH_SHIFT		(0U)
#define AFBCDEC_OUTBUF_STRIDE_SHIFT			(7U)

#define AFBCDEC_OUTBUF_ADDR_LOW_MASK \
	((u32)0x1FFFFFFU << AFBCDEC_OUTBUF_ADDR_LOW_SHIFT)
#define AFBCDEC_OUTBUF_ADDR_HIGH_MASK \
	((u32)0xFFFFU << AFBCDEC_OUTBUF_ADDR_HIGH_SHIFT)
#define AFBCDEC_OUTBUF_STRIDE_MASK \
	((u32)0x1FFU << AFBCDEC_OUTBUF_STRIDE_SHIFT)

/*
 * Prefetch Configuration Register Information
 */
#define AFBCDEC_PREFETCH_Y_SHIFT			(1U)
#define AFBCDEC_PREFETCH_X_SHIFT			(0U)

#define AFBCDEC_PREFETCH_Y_MASK		((u32)0x1U << AFBCDEC_PREFETCH_Y_SHIFT)
#define AFBCDEC_PREFETCH_X_MASK		((u32)0x1U << AFBCDEC_PREFETCH_X_SHIFT)

#define AFBCDEC_ALIGN(value, base) (((value) + ((base)-1)) & ~((base)-1))
#define AFBCDEC_BUFFER_ALIGN (128)



/* Interface APIs */
extern void VIOC_AFBCDec_GetBlockInfo(
	const void __iomem *reg,
	unsigned int *productID, unsigned int *verMaj, unsigned int *verMin,
	unsigned int *verStat);
extern void VIOC_AFBCDec_SetContiDecEnable(
	void __iomem *reg, unsigned int enable);
extern void VIOC_AFBCDec_SetSurfaceN(
	void __iomem *reg,
	VIOC_AFBCDEC_SURFACE_NUM nSurface, unsigned int enable);
extern void VIOC_AFBCDec_SetAXICacheCfg(
	void __iomem *reg, unsigned int cache);
extern void VIOC_AFBCDec_SetAXIQoSCfg(
	void __iomem *reg, unsigned int qos);
extern void VIOC_AFBCDec_SetSrcImgBase(
	void __iomem *reg,
	unsigned int base0, unsigned int base1);
extern void VIOC_AFBCDec_SetWideModeEnable(
	void __iomem *reg, unsigned int enable);
extern void VIOC_AFBCDec_SetSplitModeEnable(
	void __iomem *reg, unsigned int enable);
extern void VIOC_AFBCDec_SetYUVTransEnable(
	void __iomem *reg, unsigned int enable);
extern void VIOC_AFBCDec_SetImgFmt(
	void __iomem *reg,
	unsigned int fmt, unsigned int enable_10bit);
extern void VIOC_AFBCDec_SetImgSize(
	void __iomem *reg,
	unsigned int width, unsigned int height);
extern void VIOC_AFBCDec_GetImgSize(
	const void __iomem *reg,
	unsigned int *width, unsigned int *height);
extern void VIOC_AFBCDec_SetBoundingBox(
	void __iomem *reg,
	unsigned int minX, unsigned int maxX,
	unsigned int minY, unsigned int maxY);
extern void VIOC_AFBCDec_GetBoundingBox(
	const void __iomem *reg,
	unsigned int *minX, unsigned int *maxX,
	unsigned int *minY, unsigned int *maxY);
extern void VIOC_AFBCDec_SetOutBufBase(
	void __iomem *reg,
	unsigned int base0, unsigned int base1,
	unsigned int fmt, unsigned int width);
extern void VIOC_AFBCDec_SetBufferFlipX(
	void __iomem *reg, unsigned int enable);
extern void VIOC_AFBCDec_SetBufferFlipY(
	void __iomem *reg, unsigned int enable);
extern unsigned int VIOC_AFBCDec_GetStatus(
	const void __iomem *reg);
extern void VIOC_AFBCDec_SetIrqMask(
	void __iomem *reg, unsigned int enable, unsigned int mask);
extern void VIOC_AFBCDec_ClearIrq(
	void __iomem *reg, unsigned int mask);
extern void VIOC_AFBCDec_TurnOFF(
	void __iomem *reg);
extern void VIOC_AFBCDec_TurnOn(
	void __iomem *reg, VIOC_AFBCDEC_SWAP swapmode);
extern void VIOC_AFBCDec_SurfaceCfg(
	void __iomem *reg, unsigned int base,
	unsigned int fmt, unsigned int width, unsigned int height,
	unsigned int b10bit, unsigned int split_mode, unsigned int wide_mode,
	unsigned int nSurface, unsigned int bSetOutputBase);
extern void VIOC_AFBCDec_DUMP(
	const void __iomem *reg, unsigned int vioc_id);
extern void __iomem *VIOC_AFBCDec_GetAddress(
	unsigned int vioc_id);

#endif //__VIOC_VIQE_H__