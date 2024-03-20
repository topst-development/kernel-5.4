// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * vioc_afbc_dec.c
 * Author:  <linux@telechips.com>
 * Created:  10, 2018
 * Description: TCC VIOC h/w block
 *
 * Copyright (C) 2018 Telechips
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
#include <linux/kernel.h>
#include <linux/of_address.h>
#include <video/tcc/vioc_global.h>
#include <video/tcc/vioc_ddicfg.h>	// is_VIOC_REMAP
#include <video/tcc/vioc_afbcdec.h>

#define AFBCDec_MAX_N 2U
#define AFBC_DEC_BUFFER_ALIGN 64U
#define AFBC_ALIGNED(w, mul) ((((unsigned int)w) + ((mul) - 1U)) & ~((mul) - 1U))

static struct device_node *pViocAFBCDec_np;
static void __iomem *pAFBCDec_reg[AFBCDec_MAX_N] = {0};

/******************************* AFBC_DEC Control
 * *******************************/

void VIOC_AFBCDec_GetBlockInfo(const void __iomem *reg,
			       unsigned int *productID, unsigned int *verMaj,
			       unsigned int *verMin, unsigned int *verStat)
{
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	*productID = ((__raw_readl(reg + AFBCDEC_BLOCK_ID) &
		       AFBCDEC_PRODUCT_ID_MASK) >>
		      AFBCDEC_PRODUCT_ID_SHIFT);
	*verMaj = ((__raw_readl(reg + AFBCDEC_BLOCK_ID) &
		    AFBCDEC_VERSION_MAJOR_MASK) >>
		   AFBCDEC_VERSION_MAJOR_SHIFT);
	*verMin = ((__raw_readl(reg + AFBCDEC_BLOCK_ID) &
		    AFBCDEC_VERSION_MINOR_MASK) >>
		   AFBCDEC_VERSION_MINOR_SHIFT);
	*verStat = ((__raw_readl(reg + AFBCDEC_BLOCK_ID) &
		     AFBCDEC_VERSION_STATUS_MASK) >>
		    AFBCDEC_VERSION_STATUS_SHIFT);
}

void VIOC_AFBCDec_SetContiDecEnable(void __iomem *reg,
				    unsigned int enable)
{
	u32 val;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	val = (__raw_readl(reg + AFBCDEC_SURFACE_CFG) &
	       ~(AFBCDEC_SURCFG_CONTI_MASK));
	val |= (enable << AFBCDEC_SURCFG_CONTI_SHIFT);
	__raw_writel(val, reg + AFBCDEC_SURFACE_CFG);
}

void VIOC_AFBCDec_SetSurfaceN(void __iomem *reg,
			      VIOC_AFBCDEC_SURFACE_NUM nSurface,
			      unsigned int enable)
{
	u32 val;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	val = (__raw_readl(reg + AFBCDEC_SURFACE_CFG)
		& ~((u32)0x1U << (unsigned int)nSurface));
	val |= (enable << (unsigned int)nSurface);
	__raw_writel(val, reg + AFBCDEC_SURFACE_CFG);
}

void VIOC_AFBCDec_SetAXICacheCfg(void __iomem *reg, unsigned int cache)
{
	u32 val;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	val = (__raw_readl(reg + AFBCDEC_AXI_CFG) &
	       ~(AFBCDEC_AXICFG_CACHE_MASK));
	val |= ((cache & 0xFU) << AFBCDEC_AXICFG_CACHE_SHIFT);
	__raw_writel(val, reg + AFBCDEC_AXI_CFG);
}

void VIOC_AFBCDec_SetAXIQoSCfg(void __iomem *reg, unsigned int qos)
{
	u32 val;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	val = (__raw_readl(reg + AFBCDEC_AXI_CFG) & ~(AFBCDEC_AXICFG_QOS_MASK));
	val |= ((qos & 0xFU) << AFBCDEC_AXICFG_QOS_SHIFT);
	__raw_writel(val, reg + AFBCDEC_AXI_CFG);
}

void VIOC_AFBCDec_SetSrcImgBase(void __iomem *reg, unsigned int base0,
				unsigned int base1)
{
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	__raw_writel((base0 & 0xFFFFFFC0U), reg + AFBCDEC_S_HEADER_BUF_ADDR_LOW);
	__raw_writel((base1 & 0xFFFFU), reg + AFBCDEC_S_HEADER_BUF_ADDR_HIGH);
}

void VIOC_AFBCDec_SetWideModeEnable(void __iomem *reg,
				    unsigned int enable)
{
	u32 val;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	val = (__raw_readl(reg + AFBCDEC_S_FORMAT_SPECIFIER) &
	       ~(AFBCDEC_MODE_SUPERBLK_MASK));
	val |= (enable << AFBCDEC_MODE_SUPERBLK_SHIFT);
	__raw_writel(val, reg + AFBCDEC_S_FORMAT_SPECIFIER);
}

void VIOC_AFBCDec_SetSplitModeEnable(void __iomem *reg,
				     unsigned int enable)
{
	u32 val;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	val = (__raw_readl(reg + AFBCDEC_S_FORMAT_SPECIFIER) &
	       ~(AFBCDEC_MODE_SPLIT_MASK));
	val |= (enable << AFBCDEC_MODE_SPLIT_SHIFT);
	__raw_writel(val, reg + AFBCDEC_S_FORMAT_SPECIFIER);
}

void VIOC_AFBCDec_SetYUVTransEnable(void __iomem *reg,
				    unsigned int enable)
{
	u32 val;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	val = (__raw_readl(reg + AFBCDEC_S_FORMAT_SPECIFIER) &
	       ~(AFBCDEC_MODE_YUV_TRANSF_MASK));
	val |= (enable << AFBCDEC_MODE_YUV_TRANSF_SHIFT);
	__raw_writel(val, reg + AFBCDEC_S_FORMAT_SPECIFIER);
}

void VIOC_AFBCDec_SetImgFmt(void __iomem *reg, unsigned int fmt,
			    unsigned int enable_10bit)
{
	u32 val;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	val = (__raw_readl(reg + AFBCDEC_S_FORMAT_SPECIFIER) &
	       ~(AFBCDEC_MODE_PIXELFMT_MASK));

	switch (fmt) {
	case VIOC_IMG_FMT_RGB888:
		val |= AFBCDEC_FORMAT_RGB888;
		break;
	case VIOC_IMG_FMT_ARGB8888:
		val |= AFBCDEC_FORMAT_RGBA8888;
		break;
	case VIOC_IMG_FMT_YUYV:
		if (enable_10bit != 0U) {
			val |= AFBCDEC_FORMAT_10BIT_YUV422;
		} else {
			val |= AFBCDEC_FORMAT_8BIT_YUV422;
		}
		break;
	default:
		(void)pr_info("[INFO][AFBC] info %s fmt is %d\n",
				__func__, fmt);
		break;
	}
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	__raw_writel(val, reg + AFBCDEC_S_FORMAT_SPECIFIER);
}

void VIOC_AFBCDec_SetImgSize(void __iomem *reg, unsigned int width,
			     unsigned int height)
{
	u32 val;

	val = (((height & 0x1FFFU) << AFBCDEC_SIZE_HEIGHT_SHIFT) |
	       ((width & 0x1FFFU) << AFBCDEC_SIZE_WIDTH_SHIFT));
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	__raw_writel(val, reg + AFBCDEC_S_BUFFER_SIZE);
}

void VIOC_AFBCDec_GetImgSize(const void __iomem *reg, unsigned int *width,
			     unsigned int *height)
{
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	*width = ((__raw_readl(reg + AFBCDEC_S_BUFFER_SIZE) &
		   AFBCDEC_SIZE_WIDTH_MASK) >>
		  AFBCDEC_SIZE_WIDTH_SHIFT);
	*height = ((__raw_readl(reg + AFBCDEC_S_BUFFER_SIZE) &
		    AFBCDEC_SIZE_HEIGHT_MASK) >>
		   AFBCDEC_SIZE_HEIGHT_SHIFT);
}

void VIOC_AFBCDec_SetBoundingBox(void __iomem *reg, unsigned int minX,
				 unsigned int maxX, unsigned int minY,
				 unsigned int maxY)
{
	u32 val;

	val = (((maxX & 0x1FFFU) << AFBCDEC_BOUNDING_BOX_MAX_SHIFT) |
	       ((minX & 0x1FFFU) << AFBCDEC_BOUNDING_BOX_MIN_SHIFT));
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	__raw_writel(val, reg + AFBCDEC_S_BOUNDING_BOX_X);

	val = (((maxY & 0x1FFFU) << AFBCDEC_BOUNDING_BOX_MAX_SHIFT) |
	       ((minY & 0x1FFFU) << AFBCDEC_BOUNDING_BOX_MIN_SHIFT));
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	__raw_writel(val, reg + AFBCDEC_S_BOUNDING_BOX_Y);
}

void VIOC_AFBCDec_GetBoundingBox(const void __iomem *reg,
				 unsigned int *minX, unsigned int *maxX,
				 unsigned int *minY, unsigned int *maxY)
{
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	*minX = ((__raw_readl(reg + AFBCDEC_S_BOUNDING_BOX_X) &
		  AFBCDEC_BOUNDING_BOX_MIN_MASK) >>
		 AFBCDEC_BOUNDING_BOX_MIN_SHIFT);
	*maxX = ((__raw_readl(reg + AFBCDEC_S_BOUNDING_BOX_X) &
		  AFBCDEC_BOUNDING_BOX_MAX_MASK) >>
		 AFBCDEC_BOUNDING_BOX_MAX_SHIFT);

	*minY = ((__raw_readl(reg + AFBCDEC_S_BOUNDING_BOX_Y) &
		  AFBCDEC_BOUNDING_BOX_MIN_MASK) >>
		 AFBCDEC_BOUNDING_BOX_MIN_SHIFT);
	*maxY = ((__raw_readl(reg + AFBCDEC_S_BOUNDING_BOX_Y) &
		  AFBCDEC_BOUNDING_BOX_MAX_MASK) >>
		 AFBCDEC_BOUNDING_BOX_MAX_SHIFT);
}

void VIOC_AFBCDec_SetOutBufBase(void __iomem *reg, unsigned int base0,
				unsigned int base1, unsigned int fmt,
				unsigned int width)
{
	unsigned int afbc_stride = 0U;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	__raw_writel((base0 & 0xFFFFFF80U), reg + AFBCDEC_S_OUTPUT_BUF_ADDR_LOW);
	__raw_writel((base1 & 0xFFFFU), reg + AFBCDEC_S_OUTPUT_BUF_ADDR_HIGH);

	/* avoid CERT-C Integers Rule INT30-C */
	if (width < UINT_MAX) {
		switch (fmt) {
		case VIOC_IMG_FMT_RGB888:
		case VIOC_IMG_FMT_ARGB8888:
			afbc_stride = width * 4U;
			break;
		case VIOC_IMG_FMT_YUYV:
			afbc_stride = width * 2U;
			break;
		default:
			(void)pr_info("[INFO][AFBC] info %s fmt is %d\n",
				__func__, fmt);
			break;
		}
		__raw_writel(afbc_stride, reg + AFBCDEC_S_OUTPUT_BUF_STRIDE);
	}
}

void VIOC_AFBCDec_SetBufferFlipX(void __iomem *reg,
				 unsigned int enable)
{
	u32 val;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	val = (__raw_readl(reg + AFBCDEC_S_PREFETCH_CFG) &
	       ~(AFBCDEC_PREFETCH_X_MASK));
	val |= (enable << AFBCDEC_PREFETCH_X_SHIFT);
	__raw_writel(val, reg + AFBCDEC_S_PREFETCH_CFG);
}

void VIOC_AFBCDec_SetBufferFlipY(void __iomem *reg,
				 unsigned int enable)
{
	u32 val;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	val = (__raw_readl(reg + AFBCDEC_S_PREFETCH_CFG) &
	       ~(AFBCDEC_PREFETCH_Y_MASK));
	val |= (enable << AFBCDEC_PREFETCH_Y_SHIFT);
	__raw_writel(val, reg + AFBCDEC_S_PREFETCH_CFG);
}

void VIOC_AFBCDec_SetIrqMask(void __iomem *reg, unsigned int enable,
				unsigned int mask)
{
	if (enable == 1U) { /* Interrupt Enable*/
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		__raw_writel(mask, reg + AFBCDEC_IRQ_MASK);
	} else{ /* Interrupt Diable*/
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		__raw_writel(~mask, reg + AFBCDEC_IRQ_MASK);
	}
}

unsigned int VIOC_AFBCDec_GetStatus(const void __iomem *reg)
{
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	return __raw_readl(reg + AFBCDEC_IRQ_STATUS);
}

void VIOC_AFBCDec_ClearIrq(void __iomem *reg, unsigned int mask)
{
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	__raw_writel(mask, reg + AFBCDEC_IRQ_CLEAR);
}

void VIOC_AFBCDec_TurnOn(void __iomem *reg,
			      VIOC_AFBCDEC_SWAP swapmode)
{
	u32 val;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	val = (__raw_readl(reg + AFBCDEC_COMMAND) &
		~(AFBCDEC_CMD_DIRECT_SWAP_MASK |
		AFBCDEC_CMD_PENDING_SWAP_MASK));
	val |= ((u32)0x1U << (unsigned int)swapmode);
	__raw_writel(val, reg + AFBCDEC_COMMAND);
}

void VIOC_AFBCDec_TurnOFF(void __iomem *reg)
{
	u32 val;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	val = (__raw_readl(reg + AFBCDEC_COMMAND) &
		~(AFBCDEC_CMD_DIRECT_SWAP_MASK |
		AFBCDEC_CMD_PENDING_SWAP_MASK));
	__raw_writel(val, reg + AFBCDEC_COMMAND);
}

void VIOC_AFBCDec_SurfaceCfg(void __iomem *reg, unsigned int base,
			 unsigned int fmt, unsigned int width,
			 unsigned int height, unsigned int b10bit,
			 unsigned int split_mode, unsigned int wide_mode,
			 unsigned int nSurface, unsigned int bSetOutputBase)
{
	void __iomem *pSurface_Dec = NULL;

	if ((width < 1U) || (height < 1U)) {
		(void)pr_err("[ERR][AFBC] err %s parameter is wrong\n",
			__func__);
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto FUNC_EXIT;
	}
	//(void)pr_info("%s- Start\n", __func__);

	switch (nSurface) {
	case (unsigned int)VIOC_AFBCDEC_SURFACE_1:
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		pSurface_Dec = reg + AFBCDEC_S1_BASE;
	break;
	case (unsigned int)VIOC_AFBCDEC_SURFACE_2:
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		pSurface_Dec = reg + AFBCDEC_S2_BASE;
	break;
	case (unsigned int)VIOC_AFBCDEC_SURFACE_3:
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		pSurface_Dec = reg + AFBCDEC_S3_BASE;
	break;
	default:
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		pSurface_Dec = reg + AFBCDEC_S0_BASE;
	break;
	}

	VIOC_AFBCDec_SetSrcImgBase(pSurface_Dec, base, 0U);
	if (bSetOutputBase != 0U) {
		VIOC_AFBCDec_SetWideModeEnable(pSurface_Dec, wide_mode);
		VIOC_AFBCDec_SetSplitModeEnable(pSurface_Dec, split_mode);

		switch (fmt) {
		case VIOC_IMG_FMT_RGB888:
		case VIOC_IMG_FMT_ARGB8888:
			VIOC_AFBCDec_SetYUVTransEnable(pSurface_Dec, 1U);
		break;
		case VIOC_IMG_FMT_YUYV:
			VIOC_AFBCDec_SetYUVTransEnable(pSurface_Dec, 0U);
		break;
		default:
			(void)pr_info("[INFO][AFBC] info %s fmt is %d\n",
				__func__, fmt);
		break;
		}

		VIOC_AFBCDec_SetImgFmt(pSurface_Dec, fmt, b10bit);
		VIOC_AFBCDec_SetImgSize(pSurface_Dec, width, height);
		VIOC_AFBCDec_SetBoundingBox(pSurface_Dec, 0U, width - 1U,
								0U, height - 1U);
		VIOC_AFBCDec_SetOutBufBase(pSurface_Dec, base, 0U, fmt, width);
	}

	//(void)pr_info("%s - End\n", __func__);
FUNC_EXIT:
	return;
}

void VIOC_AFBCDec_DUMP(const void __iomem *reg, unsigned int vioc_id)
{
	unsigned int cnt = 0;
	const void __iomem *pReg = reg;
	unsigned int Num = get_vioc_index(vioc_id);

	if (Num >= AFBCDec_MAX_N) {
		(void)pr_err("[ERR][AFBC] err %s Num:%d , max :%d\n",
		__func__, Num, AFBCDec_MAX_N);
	} else {
		if (pReg == NULL) {
			/* Prevent KCS warning */
			pReg = VIOC_AFBCDec_GetAddress(vioc_id);
		}
		if (pReg != NULL) {
			(void)pr_info("[DBG][AFBC] AFBC_DEC-%d ::\n", Num);
			while (cnt < 0x100U) {
				/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
				(void)pr_info(
					"[DBG][AFBC] AFBC_DEC-%d + 0x%x: 0x%08x 0x%08x 0x%08x 0x%08x\n",
					Num, cnt,
					__raw_readl(pReg + cnt), __raw_readl(pReg + cnt + 0x4U),
					__raw_readl(pReg + cnt + 0x8U),
					__raw_readl(pReg + cnt + 0xCU));
				cnt += 0x10U;
			}
		}
	}
}

void __iomem *VIOC_AFBCDec_GetAddress(unsigned int vioc_id)
{
	unsigned int Num = get_vioc_index(vioc_id);
	void __iomem *ret = NULL;

	if ((Num >= AFBCDec_MAX_N) || (pAFBCDec_reg[Num] == NULL)) {
		(void)pr_err("[ERR][AFBC] err %s Num:%d , max :%d\n",
		__func__, Num, AFBCDec_MAX_N);
		ret = NULL;
	} else {
		ret = pAFBCDec_reg[Num];
	}

	return ret;
}

static int __init vioc_afbc_dec_init(void)
{
	int i;

	pViocAFBCDec_np =
		of_find_compatible_node(NULL, NULL, "telechips,vioc_afbc_dec");

	if (pViocAFBCDec_np == NULL) {
		/* Prevent KCS warning */
		(void)pr_info("[INF][AFBC] vioc-afbc_dec: disabled\n");
	} else {
		for (i = 0; i < (int)AFBCDec_MAX_N; i++) {
			pAFBCDec_reg[i] = (void __iomem *)of_iomap(
					pViocAFBCDec_np,
					(is_VIOC_REMAP != 0U) ?
					(i + (int)AFBCDec_MAX_N) : i);

			if (pAFBCDec_reg[i] != NULL) {
				(void)pr_info("[INF][AFBC] vioc-afbc_dec%d\n", i);
			}
		}
	}
	return 0;
}
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
arch_initcall(vioc_afbc_dec_init);
