// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
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
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/of_address.h>

#include <video/tcc/tcc_types.h>
#include <video/tcc/vioc_disp.h>
#include <video/tcc/vioc_ddicfg.h> // is_VIOC_REMAP
#include <video/tcc/vioc_global.h>

#define MAX_WAIT_CNT 0xF0000U

static void __iomem *pDISP_reg[VIOC_DISP_MAX] = {0};

#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
/*
 * DISP.DALIGN.[SWAPBF, SWAPAF] register
 * =====================================
 *
 * SWAPBF/SWAPPRE -> pxdw, r2y, y2r -> SWAPAF/SWAPPOST
 *  - SWAPBF/SWAPPRE  : Swap Before DISP (VIOC_DISP_SetSwapbf)
 *  - SWAPAF/SWAPPOST : Swap After DISP (VIOC_DISP_SetSwapaf)
 *
 * ------------------------------------------
 * Chip    | SWAPBF/SWAPPRE | SWAPAF/SWAPPOST
 * --------+----------------+----------------
 * TCC898X | bit[7:5]       | bit[4:2]
 * TCC899X | bit[7:5]       | bit[4:2]
 * TCC803X | bit[2:0]       | bit[5:3]
 * TCC805X | bit[2:0]       | bit[5:3]
 */

void VIOC_DISP_SetSwapbf(void __iomem *reg, unsigned int swapbf)
{
	u32 value;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	value = __raw_readl(reg + DALIGN) & ~(DALIGN_SWAPBF_MASK);
	value |= (swapbf << DALIGN_SWAPBF_SHIFT);
	__raw_writel(value, reg + DALIGN);
}

void VIOC_DISP_GetSwapbf(const void __iomem *reg, unsigned int *swapbf)
{
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	*swapbf = (__raw_readl(reg + DALIGN) & DALIGN_SWAPBF_MASK)
		>> DALIGN_SWAPBF_SHIFT;
}

void VIOC_DISP_SetSwapaf(void __iomem *reg, unsigned int swapaf)
{
	u32 value;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	value = __raw_readl(reg + DALIGN) & ~(DALIGN_SWAPAF_MASK);
	value |= (swapaf << DALIGN_SWAPAF_SHIFT);
	__raw_writel(value, reg + DALIGN);
}

void VIOC_DISP_GetSwapaf(const void __iomem *reg, unsigned int *swapaf)
{
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	*swapaf = (__raw_readl(reg + DALIGN) & DALIGN_SWAPAF_MASK)
		>> DALIGN_SWAPAF_SHIFT;
}
#endif

void VIOC_DISP_SetSize(
	void __iomem *reg, unsigned int nWidth, unsigned int nHeight)
{
	__raw_writel(
		(nHeight << DDS_VSIZE_SHIFT) | (nWidth << DDS_HSIZE_SHIFT),
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		reg + DDS);
}

void VIOC_DISP_GetSize(
	const void __iomem *reg, unsigned int *nWidth, unsigned int *nHeight)
{
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	*nWidth = (__raw_readl(reg + DDS) & DDS_HSIZE_MASK) >> DDS_HSIZE_SHIFT;
	*nHeight = (__raw_readl(reg + DDS) & DDS_VSIZE_MASK) >> DDS_VSIZE_SHIFT;
}

// BG0 : Red, BG1 : Green,BG2, Blue
void VIOC_DISP_SetBGColor(
	void __iomem *reg, unsigned int BG0, unsigned int BG1,
	unsigned int BG2, unsigned int BG3)
{
	u32 value;

	value =
		(((BG3 & 0xFFU) << DBC_BG3_SHIFT)
		 | ((BG2 & 0xFFU) << DBC_BG2_SHIFT)
		 | ((BG1 & 0xFFU) << DBC_BG1_SHIFT)
		 | ((BG0 & 0xFFU) << DBC_BG0_SHIFT));
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	__raw_writel(value, reg + DBC);
}

void VIOC_DISP_SetPosition(
	void __iomem *reg, unsigned int startX, unsigned int startY)
{
	__raw_writel(
		(startY << DPOS_YPOS_SHIFT) | (startX << DPOS_XPOS_SHIFT),
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		reg + DPOS);
}

void VIOC_DISP_GetPosition(
	const void __iomem *reg, unsigned int *startX, unsigned int *startY)
{
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	*startX = (__raw_readl(reg + DPOS) & DPOS_XPOS_MASK) >> DPOS_XPOS_SHIFT;
	*startY = (__raw_readl(reg + DPOS) & DPOS_YPOS_MASK) >> DPOS_YPOS_SHIFT;
}

void VIOC_DISP_SetColorEnhancement(
	void __iomem *reg, unsigned int contrast,
	unsigned int brightness, unsigned int hue)
{
	u32 value;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	value =
		(((contrast & 0xFFU) << DCENH_CONTRAST_SHIFT)
		 | ((brightness & 0xFFU) << DCENH_BRIGHT_SHIFT)
		 | ((hue & 0xFFU) << DCENH_HUE_SHIFT)
		 | ((hue != 0U) ? ((u32)0x1U << DCENH_HEN_SHIFT) : ((u32)0x0U << DCENH_HEN_SHIFT)));
	__raw_writel(value, reg + DCENH);
}

void VIOC_DISP_GetColorEnhancement(
	const void __iomem *reg, unsigned int *contrast,
	unsigned int *brightness, unsigned int *hue)
{
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	*contrast = (__raw_readl(reg + DCENH) & DCENH_CONTRAST_MASK)
		>> DCENH_CONTRAST_SHIFT;
	*brightness = (__raw_readl(reg + DCENH) & DCENH_BRIGHT_MASK)
		>> DCENH_BRIGHT_SHIFT;
	*hue = (__raw_readl(reg + DCENH) & DCENH_HUE_MASK) >> DCENH_HUE_SHIFT;
}

void VIOC_DISP_SetClippingEnable(
	void __iomem *reg, unsigned int enable)
{
	u32 value;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	value = (__raw_readl(reg + DCTRL) & DCTRL_CLEN_MASK);
	value |= (enable << DCTRL_CLEN_SHIFT);
	__raw_writel(value, reg + DCTRL);
}

void VIOC_DISP_GetClippingEnable(
	const void __iomem *reg, unsigned int *enable)
{
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	*enable = (__raw_readl(reg + DCTRL) & DCTRL_CLEN_MASK)
		>> DCTRL_CLEN_SHIFT;
}

void VIOC_DISP_SetClipping(
	void __iomem *reg, unsigned int uiUpperLimitY,
	unsigned int uiLowerLimitY, unsigned int uiUpperLimitUV,
	unsigned int uiLowerLimitUV)
{
	u32 value;

	value = (uiUpperLimitY << DCPY_CLPH_SHIFT)
		| (uiLowerLimitY << DCPY_CLPL_SHIFT);
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	__raw_writel(value, reg + DCPY);

	value = (uiUpperLimitUV << DCPC_CLPH_SHIFT)
		| (uiLowerLimitUV << DCPC_CLPL_SHIFT);
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	__raw_writel(value, reg + DCPC);
}

void VIOC_DISP_GetClipping(
	const void __iomem *reg, unsigned int *uiUpperLimitY,
	unsigned int *uiLowerLimitY, unsigned int *uiUpperLimitUV,
	unsigned int *uiLowerLimitUV)
{
	/* avoid MISRA C-2012 Rule 2.7 */
    (void)reg;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	*uiUpperLimitY =
		(__raw_readl(reg + DCPY) & DCPY_CLPH_MASK) >> DCPY_CLPH_SHIFT;
	*uiLowerLimitY =
		(__raw_readl(reg + DCPY) & DCPY_CLPL_MASK) >> DCPY_CLPL_SHIFT;
	*uiUpperLimitUV =
		(__raw_readl(reg + DCPC) & DCPC_CLPH_MASK) >> DCPC_CLPH_SHIFT;
	*uiLowerLimitUV =
		(__raw_readl(reg + DCPC) & DCPC_CLPL_MASK) >> DCPC_CLPL_SHIFT;
}

void VIOC_DISP_SetDither(
	void __iomem *reg, unsigned int ditherEn,
	unsigned int ditherSel, unsigned char mat[4][4])
{
	u32 value;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	value =
		(__raw_readl(reg + DDMAT0)
		 & ~(DDMAT0_DITH03_MASK | DDMAT0_DITH02_MASK
		     | DDMAT0_DITH01_MASK | DDMAT0_DITH00_MASK));
	value |=
		(((u32)mat[0][3] << DDMAT0_DITH03_SHIFT)
		 | ((u32)mat[0][2] << DDMAT0_DITH02_SHIFT)
		 | ((u32)mat[0][1] << DDMAT0_DITH01_SHIFT)
		 | ((u32)mat[0][0] << DDMAT0_DITH00_SHIFT));
	__raw_writel(value, reg + DDMAT0);

	value =
		(__raw_readl(reg + DDMAT0)
		 & ~(DDMAT0_DITH13_MASK | DDMAT0_DITH12_MASK
		     | DDMAT0_DITH11_MASK | DDMAT0_DITH10_MASK));
	value |=
		(((u32)mat[1][3] << DDMAT0_DITH13_SHIFT)
		 | ((u32)mat[1][2] << DDMAT0_DITH12_SHIFT)
		 | ((u32)mat[1][1] << DDMAT0_DITH11_SHIFT)
		 | ((u32)mat[1][0] << DDMAT0_DITH10_SHIFT));
	__raw_writel(value, reg + DDMAT0);

	value =
		(__raw_readl(reg + DDMAT1)
		 & ~(DDMAT1_DITH23_MASK | DDMAT1_DITH22_MASK
		     | DDMAT1_DITH21_MASK | DDMAT1_DITH20_MASK));
	value |=
		(((u32)mat[2][3] << DDMAT1_DITH23_SHIFT)
		 | ((u32)mat[2][2] << DDMAT1_DITH22_SHIFT)
		 | ((u32)mat[2][1] << DDMAT1_DITH21_SHIFT)
		 | ((u32)mat[2][0] << DDMAT1_DITH20_SHIFT));
	__raw_writel(value, reg + DDMAT1);

	value =
		(__raw_readl(reg + DDMAT1)
		 & ~(DDMAT1_DITH33_MASK | DDMAT1_DITH32_MASK
		     | DDMAT1_DITH31_MASK | DDMAT1_DITH30_MASK));
	value |=
		(((u32)mat[3][3] << DDMAT1_DITH33_SHIFT)
		 | ((u32)mat[3][2] << DDMAT1_DITH32_SHIFT)
		 | ((u32)mat[3][1] << DDMAT1_DITH31_SHIFT)
		 | ((u32)mat[3][0] << DDMAT1_DITH30_SHIFT));
	__raw_writel(value, reg + DDMAT1);

	value =
		(__raw_readl(reg + DDITH)
		 & ~(DDITH_DEN_MASK | DDITH_DSEL_MASK));
	value |=
		((ditherEn << DDITH_DEN_SHIFT)
		 | (ditherSel << DDITH_DSEL_SHIFT));
	__raw_writel(value, reg + DDITH);
}

void VIOC_DISP_SetTimingParam(void __iomem *reg, const stLTIMING *pTimeParam)
{
	u32 value;

	if ((pTimeParam->lpc < 1U) || (pTimeParam->lswc < 1U) || (pTimeParam->lewc < 1U)) {
		(void)pr_err("[ERR][DISP] %s parameter is wrong lpc(%d), lswc(%d) lewc(%d)\n",
				__func__, pTimeParam->lpc, pTimeParam->lswc, pTimeParam->lewc);
	} else {
		//	Horizon
		value =
			(((pTimeParam->lpc - 1U) << DHTIME1_LPC_SHIFT)
			| (pTimeParam->lpw << DHTIME1_LPW_SHIFT));
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		__raw_writel(value, reg + DHTIME1);

		value =
			(((pTimeParam->lswc - 1U) << DHTIME2_LSWC_SHIFT)
			| ((pTimeParam->lewc - 1U) << DHTIME2_LEWC_SHIFT));
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		__raw_writel(value, reg + DHTIME2);

		//	Vertical timing
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		value =
			(__raw_readl(reg + DVTIME1)
			& ~(DVTIME1_FLC_MASK | DVTIME1_FPW_MASK));
		value |=
			((pTimeParam->flc << DVTIME1_FLC_SHIFT)
			| (pTimeParam->fpw << DVTIME1_FPW_SHIFT));
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		__raw_writel(value, reg + DVTIME1);

		value =
			((pTimeParam->fswc << DVTIME2_FSWC_SHIFT)
			| (pTimeParam->fewc << DVTIME2_FEWC_SHIFT));
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		__raw_writel(value, reg + DVTIME2);

		value =
			((pTimeParam->flc2 << DVTIME3_FLC_SHIFT)
			| (pTimeParam->fpw2 << DVTIME3_FPW_SHIFT));
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		__raw_writel(value, reg + DVTIME3);

		value =
			((pTimeParam->fswc2 << DVTIME4_FSWC_SHIFT)
			| (pTimeParam->fewc2 << DVTIME4_FEWC_SHIFT));
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		__raw_writel(value, reg + DVTIME4);
	}
}

void VIOC_DISP_SetControlConfigure(
	void __iomem *reg, const stLCDCTR *pCtrlParam)
{
	u32 value;

	value =
		((pCtrlParam->evp << DCTRL_EVP_SHIFT)
		 | (pCtrlParam->evs << DCTRL_EVS_SHIFT)
		 | (pCtrlParam->r2ymd << DCTRL_R2YMD_SHIFT)
		 | (pCtrlParam->advi << DCTRL_ADVI_SHIFT)
		 | (pCtrlParam->ccir656 << DCTRL_656_SHIFT)
		 | (pCtrlParam->ckg << DCTRL_CKG_SHIFT)
		 | ((u32)0x1U << DCTRL_SREQ_SHIFT /* Reset default */)
		 | (pCtrlParam->pxdw << DCTRL_PXDW_SHIFT)
		 | (pCtrlParam->id << DCTRL_ID_SHIFT)
		 | (pCtrlParam->iv << DCTRL_IV_SHIFT)
		 | (pCtrlParam->ih << DCTRL_IH_SHIFT)
		 | (pCtrlParam->ip << DCTRL_IP_SHIFT)
		 | (pCtrlParam->clen << DCTRL_CLEN_SHIFT)
		 | (pCtrlParam->r2y << DCTRL_R2Y_SHIFT)
		 | (pCtrlParam->dp << DCTRL_DP_SHIFT)
		 | (pCtrlParam->ni << DCTRL_NI_SHIFT)
		 | (pCtrlParam->tv << DCTRL_TV_SHIFT)
		 | ((u32)0x1U << DCTRL_SRST_SHIFT /* Auto recovery */)
		 | (pCtrlParam->y2r << DCTRL_Y2R_SHIFT));
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	__raw_writel(value, reg + DCTRL);
}

unsigned int VIOC_DISP_FMT_isRGB(unsigned int pxdw)
{
	unsigned int ret = (unsigned int)false;

	switch (pxdw) {
	case DCTRL_FMT_4BIT:
	case DCTRL_FMT_8BIT:
	case DCTRL_FMT_8BIT_RGB_STRIPE:
	case DCTRL_FMT_16BIT_RGB565:
	case DCTRL_FMT_16BIT_RGB555:
	case DCTRL_FMT_18BIT_RGB666:
	case DCTRL_FMT_8BIT_RGB_DLETA0:
	case DCTRL_FMT_8BIT_RGB_DLETA1:
	case DCTRL_FMT_24BIT_RGB888:
	case DCTRL_FMT_8BIT_RGB_DUMMY:
	case DCTRL_FMT_16BIT_RGB666:
	case DCTRL_FMT_16BIT_RGB888:
	case DCTRL_FMT_10BIT_RGB_STRIPE:
	case DCTRL_FMT_10BIT_RGB_DELTA0:
	case DCTRL_FMT_10BIT_RGB_DELTA1:
	case DCTRL_FMT_30BIT_RGB:
	case DCTRL_FMT_10BIT_RGB_DUMMY:
	case DCTRL_FMT_20BIT_RGB:
		ret = (unsigned int)true;
		break;
	case DCTRL_FMT_8BIT_YCBCR0:
	case DCTRL_FMT_8BIT_YCBCR1:
	case DCTRL_FMT_16BIT_YCBCR0:
	case DCTRL_FMT_16BIT_YCBCR1:
	case DCTRL_FMT_10BIT_YCBCR0:
	case DCTRL_FMT_10BIT_YCBCR1:
	case DCTRL_FMT_20BIT_YCBCR0:
	case DCTRL_FMT_20BIT_YCBCR1:
	case DCTRL_FMT_24BIT_YCBCR:
	case DCTRL_FMT_30BIT_YCBCR:
		ret = (unsigned int)false;
		break;
	default:
		(void)pr_err("[ERR][DISP] %s Pixel Data Format :%d\n", __func__, pxdw);
		ret = (unsigned int)true;
		break;
	}
	return ret;
}

void VIOC_DISP_GetDisplayBlock_Info(
	const void __iomem *reg, struct DisplayBlock_Info *DDinfo)
{
	unsigned int value = 0U;

	if (reg == NULL) {
		/* Prevent KCS warning */
		(void)pr_err("[ERR][DISP] %s reg is NULL\n", __func__);
	} else {

		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		value = __raw_readl(reg + DCTRL);
		DDinfo->pCtrlParam.r2ymd =
			(value & DCTRL_R2YMD_MASK) >> DCTRL_R2YMD_SHIFT;
		DDinfo->pCtrlParam.advi = (value & DCTRL_ADVI_MASK) >> DCTRL_ADVI_SHIFT;
		DDinfo->pCtrlParam.ccir656 =
			(value & DCTRL_656_MASK) >> DCTRL_656_SHIFT;
		DDinfo->pCtrlParam.ckg = (value & DCTRL_CKG_MASK) >> DCTRL_CKG_SHIFT;
		DDinfo->pCtrlParam.pxdw = (value & DCTRL_PXDW_MASK) >> DCTRL_PXDW_SHIFT;
		DDinfo->pCtrlParam.id = (value & DCTRL_ID_MASK) >> DCTRL_ID_SHIFT;
		DDinfo->pCtrlParam.iv = (value & DCTRL_IV_MASK) >> DCTRL_IV_SHIFT;
		DDinfo->pCtrlParam.ih = (value & DCTRL_IH_MASK) >> DCTRL_IH_SHIFT;
		DDinfo->pCtrlParam.ip = (value & DCTRL_IP_MASK) >> DCTRL_IP_SHIFT;
		DDinfo->pCtrlParam.clen = (value & DCTRL_CLEN_MASK) >> DCTRL_CLEN_SHIFT;
		DDinfo->pCtrlParam.r2y = (value & DCTRL_R2Y_MASK) >> DCTRL_R2Y_SHIFT;
		DDinfo->pCtrlParam.dp = (value & DCTRL_DP_MASK) >> DCTRL_DP_SHIFT;
		DDinfo->pCtrlParam.ni = (value & DCTRL_NI_MASK) >> DCTRL_NI_SHIFT;
		DDinfo->pCtrlParam.tv = (value & DCTRL_TV_MASK) >> DCTRL_TV_SHIFT;
		DDinfo->pCtrlParam.y2r = (value & DCTRL_Y2R_MASK) >> DCTRL_Y2R_SHIFT;
		DDinfo->enable = (value & DCTRL_LEN_MASK) >> DCTRL_LEN_SHIFT;
		DDinfo->width =
			(__raw_readl(reg + DDS) & (DDS_HSIZE_MASK)) >> DDS_HSIZE_SHIFT;
		DDinfo->height =
			(__raw_readl(reg + DDS) & (DDS_VSIZE_MASK)) >> DDS_VSIZE_SHIFT;
	}
}

void VIOC_DISP_SetPXDW(void __iomem *reg, unsigned char PXDW)
{
	u32 value;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	value = (__raw_readl(reg + DCTRL) & ~(DCTRL_PXDW_MASK));
	value |= ((u32)PXDW << DCTRL_PXDW_SHIFT);
	__raw_writel(value, reg + DCTRL);
}

void VIOC_DISP_SetR2YMD(void __iomem *reg, unsigned char R2YMD)
{
	u32 value;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	value = (__raw_readl(reg + DCTRL) & ~(DCTRL_R2YMD_MASK));
	value |= ((u32)R2YMD << DCTRL_R2YMD_SHIFT);
	__raw_writel(value, reg + DCTRL);
}

void VIOC_DISP_SetR2Y(void __iomem *reg, unsigned char R2Y)
{
	u32 value;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	value = (__raw_readl(reg + DCTRL) & ~(DCTRL_R2Y_MASK));
	value |= ((u32)R2Y << DCTRL_R2Y_SHIFT);
	__raw_writel(value, reg + DCTRL);
}

void VIOC_DISP_SetY2RMD(void __iomem *reg, unsigned char Y2RMD)
{
	u32 value;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	value = (__raw_readl(reg + DCTRL) & ~(DCTRL_Y2RMD_MASK));
	value |= ((u32)Y2RMD << DCTRL_Y2RMD_SHIFT);
	__raw_writel(value, reg + DCTRL);
}

void VIOC_DISP_SetY2R(void __iomem *reg, unsigned char Y2R)
{
	u32 value;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	value = (__raw_readl(reg + DCTRL) & ~(DCTRL_Y2R_MASK));
	value |= ((u32)Y2R << DCTRL_Y2R_SHIFT);
	__raw_writel(value, reg + DCTRL);
}

void VIOC_DISP_SetSWAP(void __iomem *reg, unsigned char SWAP)
{
	u32 value;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	value = (__raw_readl(reg + DCTRL) & ~(DCTRL_SWAPBF_MASK));
	value |= ((u32)SWAP << DCTRL_SWAPBF_SHIFT);
	__raw_writel(value, reg + DCTRL);
}

void VIOC_DISP_SetCKG(void __iomem *reg, unsigned char CKG)
{
	u32 value;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	value = (__raw_readl(reg + DCTRL) & ~(DCTRL_CKG_MASK));
	value |= ((u32)CKG << DCTRL_CKG_SHIFT);
	__raw_writel(value, reg + DCTRL);
}

void VIOC_DISP_TurnOn(void __iomem *reg)
{
	u32 value;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	if ((__raw_readl(reg + DCTRL) & DCTRL_LEN_MASK) != 0U) {
		/* Prevent KCS warning */
		//(void)pr_info("[INFO][DISP] %s Display is already enabled\n", __func__);
	} else {
		value = (__raw_readl(reg + DCTRL) & ~(DCTRL_LEN_MASK));
		value |= (((u32)0x1U << DCTRL_SRST_SHIFT) | (((u32)0x1U << DCTRL_LEN_SHIFT)));
		__raw_writel(value, reg + DCTRL);
	}
}

void VIOC_DISP_TurnOff(void __iomem *reg)
{
	u32 value;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	if ((__raw_readl(reg + DCTRL) & DCTRL_LEN_MASK) == 0U) {
		/* Prevent KCS warning */
		//(void)pr_info("[INFO][DISP] %s Display is already disabled\n", __func__);
	} else {
		value = (__raw_readl(reg + DCTRL) & ~(DCTRL_LEN_MASK));
		value |= ((u32)0x0U << DCTRL_LEN_SHIFT);
		__raw_writel(value, reg + DCTRL);
	}
}

unsigned int VIOC_DISP_Get_TurnOnOff(const void __iomem *reg)
{
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	return (((__raw_readl(reg + DCTRL) & DCTRL_LEN_MASK) != 0U) ? 1U : 0U);
}

int VIOC_DISP_Wait_DisplayDone(const void __iomem *reg)
{
	unsigned int loop = 0U;
	unsigned int status = 0U;

	while (loop < MAX_WAIT_CNT) {
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		status = __raw_readl(reg + DSTATUS);
		if ((status & VIOC_DISP_IREQ_DD_MASK) == 0U) {
			/* Prevent KCS warning */
			loop++;
		} else {
			/* Prevent KCS warning */
			break;
		}
	}

	return ((int)MAX_WAIT_CNT - (int)loop);
}

int VIOC_DISP_sleep_DisplayDone(const void __iomem *reg)
{
	unsigned int loop = 0U;
	unsigned int status = 0U;

	while (loop < 20U) {
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		status = __raw_readl(reg + DSTATUS);

		if ((status & VIOC_DISP_IREQ_DD_MASK) == 0U) {
			/* Prevent KCS warning */
			loop++;
		} else {
			/* Prevent KCS warning */
			break;
		}
		usleep_range(1000, 1100);
	}
	return 0;
}

void VIOC_DISP_SetControl(void __iomem *reg, const stLCDCPARAM *pLcdParam)
{
	/* LCD Controller Stop */
	VIOC_DISP_TurnOff(reg);
	/* LCD Controller CTRL Parameter Set */
	VIOC_DISP_SetControlConfigure(reg, &pLcdParam->LCDCTRL);
	/* LCD Timing Se */
	VIOC_DISP_SetTimingParam(reg, &pLcdParam->LCDCTIMING);
	/* LCD Display Size Set */
	VIOC_DISP_SetSize(
		reg, pLcdParam->LCDCTIMING.lpc, pLcdParam->LCDCTIMING.flc);
	/* LCD Controller Enable */
	VIOC_DISP_TurnOn(reg);
}

/* set 1 : IREQ Masked( interrupt disable), set 0 : IREQ UnMasked( interrput
 * enable)
 */
void VIOC_DISP_SetIreqMask(
	void __iomem *reg, unsigned int mask, unsigned int set)
{
	if (set == 0U) { /* Interrupt Enable*/
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		__raw_writel((__raw_readl(reg + DIM) & ~(mask)), reg + DIM);
	} else { /* Interrupt Diable*/
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		__raw_writel(
			((__raw_readl(reg + DIM) & ~(mask)) | mask), reg + DIM);
	}
}

/* set 1 : IREQ Masked( interrupt disable), set 0 : IREQ UnMasked( interrput
 * enable)
 */
void VIOC_DISP_SetStatus(void __iomem *reg, unsigned int set)
{
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	__raw_writel(set, reg + DSTATUS);
}

void VIOC_DISP_GetStatus(const void __iomem *reg, unsigned int *status)
{
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	*status = __raw_readl(reg + DSTATUS);
}

void VIOC_DISP_EmergencyFlagDisable(void __iomem *reg)
{
	u32 value;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	value = (__raw_readl(reg + DEFR) & ~(DEFR_MEN_MASK));
	value |= ((u32)0x3U << DEFR_MEN_SHIFT);
	__raw_writel(value, reg + DEFR);
}

void VIOC_DISP_EmergencyFlag_SetEofm(
	void __iomem *reg, unsigned int eofm)
{
	u32 value;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	value = (__raw_readl(reg + DEFR) & ~(DEFR_EOFM_MASK));
	value |= ((eofm & 0x3U) << DEFR_EOFM_SHIFT);
	__raw_writel(value, reg + DEFR);
}

void VIOC_DISP_EmergencyFlag_SetHdmiVs(
	void __iomem *reg, unsigned int hdmivs)
{
	u32 value;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	value = (__raw_readl(reg + DEFR) & ~(DEFR_HDMIVS_MASK));
	value |= ((hdmivs & 0x3U) << DEFR_HDMIVS_SHIFT);
	__raw_writel(value, reg + DEFR);
}

unsigned int vioc_disp_get_clkdiv(const void __iomem *reg)
{
	unsigned int value;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	value = (__raw_readl(reg + DCLKDIV) & (DCLKDIV_PXCLKDIV_MASK))
		>> DCLKDIV_PXCLKDIV_SHIFT;
	return value;
}

void vioc_disp_set_clkdiv(void __iomem *reg, unsigned int divide)
{
	u32 value;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	value = (__raw_readl(reg + DCLKDIV) & ~(DCLKDIV_PXCLKDIV_MASK));
	value |= ((divide << DCLKDIV_PXCLKDIV_SHIFT) & DCLKDIV_PXCLKDIV_MASK);
	__raw_writel(value, reg + DCLKDIV);
}

void VIOC_DISP_DUMP(const void __iomem *reg, unsigned int vioc_id)
{
	unsigned int cnt = 0;
	const void __iomem *pReg = reg;
	unsigned int Num = get_vioc_index(vioc_id);

	if (Num >= VIOC_DISP_MAX) {
		(void)pr_err("[ERR][DISP] %s Num:%d , max :%d\n", __func__, Num,
			VIOC_DISP_MAX);
	} else {
		if (reg == NULL) {
			pReg = VIOC_DISP_GetAddress(vioc_id);
		}
		if (pReg != NULL) {
			(void)pr_info("[DBG][DISP] DISP-%d ::\n", Num);
			while (cnt < 0x60U) {
				/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
				(void)pr_info(
					"[DBG][DISP] DISP-%d + 0x%x: 0x%08x 0x%08x 0x%08x 0x%08x\n",
					Num, cnt, __raw_readl(pReg + cnt),
					__raw_readl(pReg + cnt + 0x4U),
					__raw_readl(pReg + cnt + 0x8U),
					__raw_readl(pReg + cnt + 0xCU));
				cnt += 0x10U;
			}
		}
	}
}

void __iomem *VIOC_DISP_GetAddress(unsigned int vioc_id)
{
	unsigned int Num = get_vioc_index(vioc_id);
	void __iomem *ret = NULL;

	if ((Num >= VIOC_DISP_MAX) || (pDISP_reg[Num] == NULL)) {
		(void)pr_err("[ERR][DISP] %s Num:%d , max :%d\n", __func__, Num,
			VIOC_DISP_MAX);
		ret = NULL;
	} else {
		ret = pDISP_reg[Num];
	}

	return ret;
}

static int __init vioc_disp_init(void)
{
	unsigned int i;
	struct device_node *ViocDisp_np;

	ViocDisp_np =
		of_find_compatible_node(NULL, NULL, "telechips,vioc_disp");
	if (ViocDisp_np == NULL) {
		/* Prevent KCS warning */
		(void)pr_info("[INF][DISP] disabled [this is mandatory for vioc display]\n");
	} else {
		for (i = 0; i < VIOC_DISP_MAX; i++) {
			pDISP_reg[i] = (void __iomem *)of_iomap(
				ViocDisp_np,
				(is_VIOC_REMAP != 0U) ? ((int)i + (int)VIOC_DISP_MAX) : (int)i);

			if (pDISP_reg[i] != NULL) {
				/* Prevent KCS warning */
				(void)pr_info("[INF][DISP] vioc-disp%d\n", i);
			}
		}
	}

	return 0;
}
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
arch_initcall(vioc_disp_init);
