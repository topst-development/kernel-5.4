// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * linux/arch/arm/mach-tcc893x/vioc_wdma.c
 * Author:  <linux@telechips.com>
 * Created: June 10, 2008
 * Description: TCC VIOC h/w block
 *
 * Copyright (C) 2008-2009 Telechips
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

#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_address.h>

#include <video/tcc/tcc_types.h>
#include <video/tcc/vioc_wdma.h>
#include <video/tcc/vioc_global.h>
#include <video/tcc/vioc_ddicfg.h> // is_VIOC_REMAP

static struct device_node *ViocWdma_np;
static void __iomem *pWDMA_reg[VIOC_WDMA_MAX] = {0};

void VIOC_WDMA_SetImageEnable(
	void __iomem *reg, unsigned int nContinuous)
{
	u32 value;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	value =
		(__raw_readl(reg + WDMACTRL_OFFSET)
		 & ~(WDMACTRL_IEN_MASK | WDMACTRL_CONT_MASK
		     | WDMACTRL_UPD_MASK));
	/*
	 * redundant update UPD has problem
	 * So if UPD is high, do not update UPD bit.
	 */
	value |=
		(((u32)0x1U << WDMACTRL_IEN_SHIFT)
		 | (nContinuous << WDMACTRL_CONT_SHIFT)
		 | ((u32)0x1U << WDMACTRL_UPD_SHIFT));

	__raw_writel(value, reg + WDMACTRL_OFFSET);
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_WDMA_SetImageEnable);

void VIOC_WDMA_GetImageEnable(const void __iomem *reg, unsigned int *enable)
{
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	*enable =
		((__raw_readl(reg + WDMACTRL_OFFSET) & WDMACTRL_IEN_MASK)
		 >> WDMACTRL_IEN_SHIFT);
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_WDMA_GetImageEnable);

void VIOC_WDMA_SetImageDisable(void __iomem *reg)
{
	u32 value;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	value =
		(__raw_readl(reg + WDMACTRL_OFFSET)
		 & ~(WDMACTRL_IEN_MASK | WDMACTRL_UPD_MASK));
	value |= (((u32)0x0U << WDMACTRL_IEN_SHIFT) |
		((u32)0x1U << WDMACTRL_UPD_SHIFT));
	__raw_writel(value, reg + WDMACTRL_OFFSET);
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_WDMA_SetImageDisable);

void VIOC_WDMA_SetImageUpdate(void __iomem *reg)
{
	u32 value;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	value = (__raw_readl(reg + WDMACTRL_OFFSET) & ~(WDMACTRL_UPD_MASK));
	value |= ((u32)0x1U << WDMACTRL_UPD_SHIFT);
	__raw_writel(value, reg + WDMACTRL_OFFSET);
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_WDMA_SetImageUpdate);

void VIOC_WDMA_SetContinuousMode(
	void __iomem *reg, unsigned int enable)
{
	u32 value;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	value = (__raw_readl(reg + WDMACTRL_OFFSET) & ~(WDMACTRL_CONT_MASK));
	value |= (enable << WDMACTRL_CONT_SHIFT);

	__raw_writel(value, reg + WDMACTRL_OFFSET);
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_WDMA_SetContinuousMode);


void VIOC_WDMA_SetImageFormat(void __iomem *reg, unsigned int nFormat)
{
	u32 value;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	value = (__raw_readl(reg + WDMACTRL_OFFSET) & ~(WDMACTRL_FMT_MASK));
	value |= (nFormat << WDMACTRL_FMT_SHIFT);

	__raw_writel(value, reg + WDMACTRL_OFFSET);
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_WDMA_SetImageFormat);

void VIOC_WDMA_SetImageRGBSwapMode(
	void __iomem *reg, unsigned int rgb_mode)
{
	u32 value;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	value = (__raw_readl(reg + WDMACTRL_OFFSET) & ~(WDMACTRL_SWAP_MASK));
	value |= (rgb_mode << WDMACTRL_SWAP_SHIFT);

	__raw_writel(value, reg + WDMACTRL_OFFSET);
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_WDMA_SetImageRGBSwapMode);


void VIOC_WDMA_SetImageInterlaced(void __iomem *reg, unsigned int intl)
{
	u32 value;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	value = (__raw_readl(reg + WDMACTRL_OFFSET) & ~(WDMACTRL_INTL_MASK));
	value |= (intl << WDMACTRL_INTL_SHIFT);

	__raw_writel(value, reg + WDMACTRL_OFFSET);
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_WDMA_SetImageInterlaced);


void VIOC_WDMA_SetImageR2YMode(
	void __iomem *reg, unsigned int r2y_mode)
{
	u32 value;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	value = (__raw_readl(reg + WDMACTRL_OFFSET) & ~(WDMACTRL_R2YMD_MASK));
	value |= (r2y_mode << WDMACTRL_R2YMD_SHIFT);

	__raw_writel(value, reg + WDMACTRL_OFFSET);
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_WDMA_SetImageR2YMode);

void VIOC_WDMA_SetImageR2YEnable(
	void __iomem *reg, unsigned int enable)
{
	u32 value;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	value = (__raw_readl(reg + WDMACTRL_OFFSET) & ~(WDMACTRL_R2Y_MASK));
	value |= (enable << WDMACTRL_R2Y_SHIFT);

	__raw_writel(value, reg + WDMACTRL_OFFSET);
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_WDMA_SetImageR2YEnable);


void VIOC_WDMA_SetImageY2RMode(
	void __iomem *reg, unsigned int y2r_mode)
{
	u32 value;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	value = (__raw_readl(reg + WDMACTRL_OFFSET) & ~(WDMACTRL_Y2RMD_MASK));
	value |= (y2r_mode << WDMACTRL_Y2RMD_SHIFT);

	__raw_writel(value, reg + WDMACTRL_OFFSET);
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_WDMA_SetImageY2RMode);

void VIOC_WDMA_SetImageY2REnable(
	void __iomem *reg, unsigned int enable)
{
	u32 value;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	value = (__raw_readl(reg + WDMACTRL_OFFSET) & ~(WDMACTRL_Y2R_MASK));
	value |= (enable << WDMACTRL_Y2R_SHIFT);

	__raw_writel(value, reg + WDMACTRL_OFFSET);
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_WDMA_SetImageY2REnable);

void VIOC_WDMA_SetImageSize(
	void __iomem *reg, unsigned int sw, unsigned int sh)
{
	u32 value;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	value = ((sh << WDMASIZE_HEIGHT_SHIFT) | (sw << WDMASIZE_WIDTH_SHIFT));
	__raw_writel(value, reg + WDMASIZE_OFFSET);
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_WDMA_SetImageSize);

void VIOC_WDMA_SetImageBase(
	void __iomem *reg, unsigned int nBase0, unsigned int nBase1,
	unsigned int nBase2)
{
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	__raw_writel(nBase0 << WDMABASE0_BASE0_SHIFT, reg + WDMABASE0_OFFSET);
	__raw_writel(nBase1 << WDMABASE1_BASE1_SHIFT, reg + WDMABASE1_OFFSET);
	__raw_writel(nBase2 << WDMABASE2_BASE2_SHIFT, reg + WDMABASE2_OFFSET);
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_WDMA_SetImageBase);

void VIOC_WDMA_SetImageOffset(
	void __iomem *reg, unsigned int imgFmt, unsigned int imgWidth)
{
	unsigned int offset0 = 0;
	unsigned int offset1 = 0;
	u32 value = 0;

	if (imgWidth > 0x1FFFU) {
		(void)pr_err("[ERR][WMIX] %s imgWidth(%d) is wrong\n", __func__, imgWidth);
	} else {
		switch (imgFmt) {
		case (unsigned int)TCC_LCDC_IMG_FMT_1BPP: // 1bpp indexed color
			offset0 = (1U * imgWidth) / 8U;
			break;
		case (unsigned int)TCC_LCDC_IMG_FMT_2BPP: // 2bpp indexed color
			offset0 = (1U * imgWidth) / 4U;
			break;
		case (unsigned int)TCC_LCDC_IMG_FMT_4BPP: // 4bpp indexed color
			offset0 = (1U * imgWidth) / 2U;
			break;
		case (unsigned int)TCC_LCDC_IMG_FMT_8BPP: // 8bpp indexed color
			offset0 = (1U * imgWidth);
			break;
		case (unsigned int)TCC_LCDC_IMG_FMT_RGB332: // RGB332 - 1bytes aligned -
						// R[7:5],G[4:2],B[1:0]
			offset0 = 1U * imgWidth;
			break;
		case (unsigned int)TCC_LCDC_IMG_FMT_RGB444: // RGB444 - 2bytes aligned -
						// A[15:12],R[11:8],G[7:3],B[3:0]
		case (unsigned int)TCC_LCDC_IMG_FMT_RGB565: // RGB565 - 2bytes aligned -
						// R[15:11],G[10:5],B[4:0]
		case (unsigned int)TCC_LCDC_IMG_FMT_RGB555: // RGB555 - 2bytes aligned -
						// A[15],R[14:10],G[9:5],B[4:0]
			offset0 = 2U * imgWidth;
			break;
		// case (unsigned int)TCC_LCDC_IMG_FMT_RGB888:
		case (unsigned int)TCC_LCDC_IMG_FMT_RGB888: // RGB888 - 4bytes aligned -
						// A[31:24],R[23:16],G[15:8],B[7:0]
		case (unsigned int)TCC_LCDC_IMG_FMT_RGB666: // RGB666 - 4bytes aligned -
						// A[23:18],R[17:12],G[11:6],B[5:0]
			offset0 = 4U * imgWidth;
			break;
		case (unsigned int)TCC_LCDC_IMG_FMT_RGB888_3: // RGB888 - 3 bytes aligned :
						// B1[31:24],R0[23:16],G0[15:8],B0[7:0]
		case (unsigned int)TCC_LCDC_IMG_FMT_ARGB6666_3: // ARGB6666 - 3 bytes aligned :
						// A[23:18],R[17:12],G[11:6],B[5:0]
			offset0 = 3U * imgWidth;
			break;
		case (unsigned int)TCC_LCDC_IMG_FMT_444SEP: /* YUV444 or RGB444 Format */
			offset0 = imgWidth;
			offset1 = imgWidth;
			break;
		case (unsigned int)TCC_LCDC_IMG_FMT_YUV420SP: // YCbCr 4:2:0 Separated format - Not
						// Supported for Image 1 and 2
			offset0 = imgWidth;
			offset1 = imgWidth / 2U;
			break;
		case (unsigned int)TCC_LCDC_IMG_FMT_YUV422SP: // YCbCr 4:2:2 Separated format - Not
						// Supported for Image 1 and 2
			offset0 = imgWidth;
			offset1 = imgWidth / 2U;
			break;
		case (unsigned int)TCC_LCDC_IMG_FMT_UYVY: // YCbCr 4:2:2 Sequential format
		case (unsigned int)TCC_LCDC_IMG_FMT_VYUY: // YCbCr 4:2:2 Sequential format
		case (unsigned int)TCC_LCDC_IMG_FMT_YUYV: // YCbCr 4:2:2 Sequential format
		case (unsigned int)TCC_LCDC_IMG_FMT_YVYU: // YCbCr 4:2:2 Sequential format
			offset0 = 2U * imgWidth;
			break;
		case (unsigned int)TCC_LCDC_IMG_FMT_YUV420ITL0: // YCbCr 4:2:0 interleved type 0
						// format - Not Supported for Image 1
						// and 2
		case (unsigned int)TCC_LCDC_IMG_FMT_YUV420ITL1: // YCbCr 4:2:0 interleved type 1
						// format - Not Supported for Image 1
						// and 2
			offset0 = imgWidth;
			offset1 = imgWidth;
			break;
		case (unsigned int)TCC_LCDC_IMG_FMT_YUV422ITL0: // YCbCr 4:2:2 interleved type 0
						// format - Not Supported for Image 1
						// and 2
		case (unsigned int)TCC_LCDC_IMG_FMT_YUV422ITL1: // YCbCr 4:2:2 interleved type 1
						// format - Not Supported for Image 1
						// and 2
			offset0 = imgWidth;
			offset1 = imgWidth;
			break;
		default:
			offset0 = imgWidth;
			offset1 = imgWidth;
			break;
		}

		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		value =
			(__raw_readl(reg + WDMAOFFS_OFFSET)
			& ~(WDMAOFFS_OFFSET1_MASK | WDMAOFFS_OFFSET0_MASK));
		value |=
			((offset1 << WDMAOFFS_OFFSET1_SHIFT)
			| (offset0 << WDMAOFFS_OFFSET0_SHIFT));
		__raw_writel(value, reg + WDMAOFFS_OFFSET);
	}
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_WDMA_SetImageOffset);

#ifdef L_STRIDE_ALIGN
void VIOC_WDMA_SetImageOffset_withYV12(
	void __iomem *reg, unsigned int imgWidth)
{
	unsigned int stride, stride_c;
	u32 value;

	stride = ALIGNED_BUFF(imgWidth, L_STRIDE_ALIGN);
	stride_c = ALIGNED_BUFF((stride / 2U), C_STRIDE_ALIGN);

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	value =
		(__raw_readl(reg + WDMAOFFS_OFFSET)
		 & ~(WDMAOFFS_OFFSET1_MASK | WDMAOFFS_OFFSET0_MASK));
	value |=
		((stride_c << WDMAOFFS_OFFSET1_SHIFT)
		 | (stride << WDMAOFFS_OFFSET0_SHIFT));
	__raw_writel(value, reg + WDMAOFFS_OFFSET);
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_WDMA_SetImageOffset_withYV12);
#endif

void VIOC_WDMA_SetIreqMask(
	void __iomem *reg, unsigned int mask, unsigned int set)
{
	/*
	 * set 1 : IREQ Masked(interrupt disable),
	 * set 0 : IREQ UnMasked(interrput enable)
	 */
	u32 value;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	value = (__raw_readl(reg + WDMAIRQMSK_OFFSET) & ~(mask));

	if (set == 1U) { /* Interrupt Disable*/
		/* Prevent KCS warning */
		value |= mask;
	}

	__raw_writel(value, reg + WDMAIRQMSK_OFFSET);
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_WDMA_SetIreqMask);

void VIOC_WDMA_SetIreqStatus(void __iomem *reg, unsigned int mask)
{
	u32 value;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	value = (__raw_readl(reg + WDMAIRQSTS_OFFSET) & ~(mask));
	value |= mask;
	__raw_writel(value, reg + WDMAIRQSTS_OFFSET);
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_WDMA_SetIreqStatus);

void VIOC_WDMA_ClearEOFR(void __iomem *reg)
{
	u32 value;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	value =
		(__raw_readl(reg + WDMAIRQSTS_OFFSET)
		 & ~(WDMAIRQSTS_EOFR_MASK));
	value |= ((u32)0x1U << WDMAIRQSTS_EOFR_SHIFT);
	__raw_writel(value, reg + WDMAIRQSTS_OFFSET);
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_WDMA_ClearEOFR);

void VIOC_WDMA_ClearEOFF(void __iomem *reg)
{
	u32 value;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	value =
		(__raw_readl(reg + WDMAIRQSTS_OFFSET)
		 & ~(WDMAIRQSTS_EOFF_MASK));
	value |= ((u32)0x1U << WDMAIRQSTS_EOFF_SHIFT);
	__raw_writel(value, reg + WDMAIRQSTS_OFFSET);
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_WDMA_ClearEOFF);

void VIOC_WDMA_GetStatus(const void __iomem *reg, unsigned int *status)
{
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	*status = __raw_readl(reg + WDMAIRQSTS_OFFSET);
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_WDMA_GetStatus);


bool VIOC_WDMA_IsImageEnable(const void __iomem *reg)
{
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	return ((((__raw_readl(reg + WDMACTRL_OFFSET) & WDMACTRL_IEN_MASK)
		>> WDMACTRL_IEN_SHIFT) != 0U) ? true : false);
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_WDMA_IsImageEnable);

bool VIOC_WDMA_IsContinuousMode(const void __iomem *reg)
{
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	return ((((__raw_readl(reg + WDMACTRL_OFFSET) & WDMACTRL_CONT_MASK)
		>> WDMACTRL_CONT_SHIFT) != 0U) ? true : false);
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_WDMA_IsContinuousMode);

unsigned int VIOC_WDMA_Get_CAddress(const void __iomem *reg)
{
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	return (__raw_readl(reg + WDMACADDR_OFFSET));
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_WDMA_Get_CAddress);

void __iomem *VIOC_WDMA_GetAddress(unsigned int vioc_id)
{
	unsigned int Num = get_vioc_index(vioc_id);
	void __iomem *ret = NULL;

	if ((Num >= VIOC_WDMA_MAX) || (pWDMA_reg[Num] == NULL)) {
		(void)pr_err("[ERR][WDMA] %s num:%d Max wdma num:%d\n",
			__func__, Num, VIOC_WDMA_MAX);
		ret = NULL;
	} else {
		ret = pWDMA_reg[Num];
	}

	return ret;
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_WDMA_GetAddress);

void VIOC_WDMA_DUMP(const void __iomem *reg, unsigned int vioc_id)
{
	unsigned int cnt = 0;
	const void __iomem *pReg = reg;
	unsigned int Num = get_vioc_index(vioc_id);

	if (Num >= VIOC_WDMA_MAX) {
		(void)pr_err("[ERR][WDMA] %s num:%d Max wdma num:%d\n",
		__func__, Num, VIOC_WDMA_MAX);
	} else {
		if (pReg == NULL) {
			/* Prevent KCS warning */
			pReg = VIOC_WDMA_GetAddress(vioc_id);
		}
		if (pReg != NULL) {
			(void)pr_info("[DBG][WDMA] WDMA-%d ::\n", Num);
			while (cnt < 0x70U) {

				/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
				(void)pr_info(
					"WDMA-%d + 0x%x: 0x%08x 0x%08x 0x%08x 0x%08x\n", Num, cnt,
					__raw_readl(pReg + cnt), __raw_readl(pReg + cnt + 0x4U),
					__raw_readl(pReg + cnt + 0x8U),
					__raw_readl(pReg + cnt + 0xCU));
				cnt += 0x10U;
			}
		}
	}
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_WDMA_DUMP);

static int __init vioc_wdma_init(void)
{
	unsigned int i = 0;

	ViocWdma_np =
		of_find_compatible_node(NULL, NULL, "telechips,vioc_wdma");
	if (ViocWdma_np == NULL) {
		/* Prevent KCS warning */
		(void)pr_info("[INF][WDMA] vioc-wdma: disabled\n");
	} else {
		for (i = 0; i < VIOC_WDMA_MAX; i++) {
			pWDMA_reg[i] = (void __iomem *)of_iomap(
				ViocWdma_np,
				(is_VIOC_REMAP != 0U) ? ((int)i + (int)VIOC_WDMA_MAX) : (int)i);

			if (pWDMA_reg[i] != NULL) {
				/* Prevent KCS warning */
				(void)pr_info("[INF][WDMA] vioc-wdma%d\n", i);
			}
		}
	}

	return 0;
}
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
arch_initcall(vioc_wdma_init);
/* EOF */