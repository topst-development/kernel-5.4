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
#include <video/tcc/vioc_rdma.h>
#include <video/tcc/vioc_global.h>
#include <video/tcc/vioc_ddicfg.h> // is_VIOC_REMAP

#include <video/tcc/vioc_config.h>
#include <video/tcc/vioc_scaler.h>

#define VIOC_RDMA_IREQ_SRC_MAX 7U

static struct device_node *ViocRdma_np;
static void __iomem *pRDMA_reg[VIOC_RDMA_MAX] = {0};

#if defined(CONFIG_ARCH_TCC805X)
static int VRDMAS[VIOC_RDMA_MAX] = {
	0, 0, 0, 1, /* RDMA3 is VRDMA */
	0, 0, 0, 1, /* RDMA7 is VRDMA */
	0, 0, 1, 1, /* RDMA10 and RDMA11 are VRDMA */
	1, 1, 1, 1, /* RDMA12 -- RDMA15 are VRDMA */
	1, 1,       /* RDMA16 and RDMA17 are VRDMA */
};
static int IMAGE_NUM[VIOC_RDMA_MAX] = {
	0, 1, 2, 3, /* RDMA00 - RDMA03 */
	0, 1, 2, 3, /* RDMA04 - RDMA07 */
	0, 1, 2, 3, /* RDMA08 - RDMA11 */
	0, 1, /* RDMA12 - RDMA13 */
	0, 1, /* RDMA14 - RDMA15 */
	1, /* RDMA16 */
	1, /* RDMA17 */
};
#endif

#if defined(CONFIG_ARCH_TCC803X)
static int VRDMAS[VIOC_RDMA_MAX] = {
	0, 0, 0, 1, /* RDMA3 is VRDMA */
	0, 0, 0, 1, /* RDMA7 is VRDMA */
	0, 0, 0, 1, /* RDMA11 are VRDMA */
	1, 1, 1, 1, /* RDMA12 -- RDMA15 are VRDMA */
	1, 1,       /* RDMA16 and RDMA17 are VRDMA */
};
static int IMAGE_NUM[VIOC_RDMA_MAX] = {
	0, 1, 2, 3, /* RDMA00 - RDMA03 */
	0, 1, 2, 3, /* RDMA04 - RDMA07 */
	0, 1, 2, 3, /* RDMA08 - RDMA11 */
	0, 1, /* RDMA12 - RDMA13 */
	0, 1, /* RDMA14 - RDMA15 */
	1, /* RDMA16 */
	1, /* RDMA17 */
};
#endif

#if defined(CONFIG_ARCH_TCC897X)
static int VRDMAS[VIOC_RDMA_MAX] = {
	0, 0, 0, 1, /* RDMA3 is VRDMA */
	0, 0, 0, 1, /* RDMA7 is VRDMA */
	0, 0, 0, 0,
	0, 1, 0, 1, /* RDMA13 and RDMA15 are VRDMA */
	1,       /* RDMA16 is VRDMA */
};
static int IMAGE_NUM[VIOC_RDMA_MAX] = {
	0, 1, 2, 3, /* RDMA00 - RDMA03 */
	0, 1, -1, 3, /* RDMA04 - RDMA07, RDMA06 NON-use */
	-1, -1, -1, -1, /* RDMA08 - RDMA11, NON-use */
	0, 1, /* RDMA12 - RDMA13 */
	0, 1, /* RDMA14 - RDMA15 */
	1, /* RDMA16 */
	-1, /* RDMA17, NON-use */
};
#endif

#define NOP __asm("NOP")

void VIOC_RDMA_SetImageUpdate(void __iomem *reg)
{
	u32 val;
	unsigned int sw = 0U, sh = 0U;
	unsigned int image_enable = 0U;

	VIOC_RDMA_GetImageSize(reg, &sw, &sh);
	VIOC_RDMA_GetImageEnable(reg, &image_enable);
	if ((sw == 0U) || (sh == 0U)) {
		if (image_enable != 0U) {
			/* This code prevents fifo underrun */
			(void)pr_err("[ERR][RDMA] %s imgWidth(%d) or imgHeight(%d) is zero\n",
					__func__, sw, sh);
  		} else {
			/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
			val = (__raw_readl(reg + RDMACTRL) & ~(RDMACTRL_UPD_MASK));
			val |= ((u32)0x1U << RDMACTRL_UPD_SHIFT);
			__raw_writel(val, reg + RDMACTRL);
		}
	} else {
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		val = (__raw_readl(reg + RDMACTRL) & ~(RDMACTRL_UPD_MASK));
		val |= ((u32)0x1U << RDMACTRL_UPD_SHIFT);
		__raw_writel(val, reg + RDMACTRL);
	}
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_RDMA_SetImageUpdate);

void VIOC_RDMA_SetImageEnable(void __iomem *reg)
{
	u32 val;
	unsigned int sw = 0U, sh = 0U;

	VIOC_RDMA_GetImageSize(reg, &sw, &sh);
	if ((sw == 0U) || (sh == 0U)) {
		/* This code prevents fifo underrun */
		(void)pr_err("[ERR][RDMA] %s imgWidth(%d) or imgHeight(%d) is zero\n",
			__func__, sw, sh);
	} else {
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		val = (__raw_readl(reg + RDMACTRL)
		       & ~(RDMACTRL_IEN_MASK | RDMACTRL_UPD_MASK));
		val |= (((u32)0x1U << RDMACTRL_IEN_SHIFT) | ((u32)0x1U << RDMACTRL_UPD_SHIFT));
		__raw_writel(val, reg + RDMACTRL);
	}
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_RDMA_SetImageEnable);

void VIOC_RDMA_GetImageEnable(const void __iomem *reg, unsigned int *enable)
{
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	*enable =
		((__raw_readl(reg + RDMACTRL) & RDMACTRL_IEN_MASK)
		 >> RDMACTRL_IEN_SHIFT);
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_RDMA_GetImageEnable);

void VIOC_RDMA_SetImageDisable(void __iomem *reg)
{
	u32 val;
	unsigned int i;
	int scaler_blknum = -1;
	int vioc_id = -1;

	/* Check RDMA is enabled */
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	if ((__raw_readl(reg + RDMACTRL) & RDMACTRL_IEN_MASK) == 0U) {
		(void)pr_info("[INF][RDMA] disabled\n");
	} else {
		for (i = 0U; i < VIOC_RDMA_MAX; i++) {
			if (pRDMA_reg[i] == reg) {
				vioc_id = (int)i;
				break;
			}
		}
		if (vioc_id >= 0) {
			/* Prevent KCS warning */
			vioc_id = vioc_id + (int)VIOC_RDMA;

			// prevent Fifo underrun
			scaler_blknum = VIOC_CONFIG_GetScaler_PluginToRDMA((unsigned int)vioc_id);
			if (scaler_blknum >= (int)VIOC_SCALER0) {
				void __iomem *pSC = VIOC_SC_GetAddress((unsigned int)scaler_blknum);

				VIOC_SC_SetDstSize(pSC, 0, 0);
				VIOC_SC_SetOutSize(pSC, 0, 0);
			}
		}

		val = (__raw_readl(reg + RDMASTAT) & ~(RDMASTAT_EOFR_MASK));
		val |= ((u32)0x1U << RDMASTAT_EOFR_SHIFT);
		__raw_writel(val, reg + RDMASTAT);

		val = (__raw_readl(reg + RDMACTRL)
			& ~(RDMACTRL_IEN_MASK | RDMACTRL_UPD_MASK));
		val |= ((u32)0x1U << RDMACTRL_UPD_SHIFT);
		__raw_writel(val, reg + RDMACTRL);

		/* This code prevents fifo underrun in the example below:
		* 1) PATH - R(1920x1080) - S(720x480) - W(720x480) - D(720x480)
		* 2) Disable RDMA.
		* 3) Unplug Scaler then fifo underrun is occurred Immediately.
		* - Because the WMIX read size from RDMA even if RDMA was disabled
		*/
		__raw_writel(0x00000000, reg + RDMASIZE);
		__raw_writel(val, reg + RDMACTRL);

		/*
		* RDMA Scale require to check STD_DEVEOF status bit, For synchronous
		* updating with EOF Status Channel turn off when scaler changed, so now
		* blocking.
		*/

		/* Wait for EOF */
		for (i = 0U; i < 60U; i++) {
			/* coverity[misra_c_2012_rule_10_1_violation : FALSE] */
			/* coverity[misra_c_2012_rule_10_4_violation : FALSE] */
			/* coverity[misra_c_2012_rule_12_1_violation : FALSE] */
			/* coverity[misra_c_2012_rule_14_4_violation : FALSE] */
			/* coverity[misra_c_2012_rule_15_6_violation : FALSE] */
			/* coverity[cert_dcl37_c_violation : FALSE] */
			mdelay(1);
			if ((__raw_readl(reg + RDMASTAT) & RDMASTAT_SDEOF_MASK) != 0U) {
				/* Prevent KCS warning */
				break;
			}
		}
		if (i == 60U) {
			(void)pr_err("[ERR][RDMA] %s : [RDMA:%d] is not disabled, RDMASTAT.nREG = 0x%08lx, CTRL 0x%08lx i = %d\n",
				__func__, vioc_id,
				(unsigned long)(__raw_readl(reg + RDMASTAT)),
				(unsigned long)(__raw_readl(reg + RDMACTRL)), i);
		}
	}
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_RDMA_SetImageDisable);

// disable and no wait
void VIOC_RDMA_SetImageDisableNW(void __iomem *reg)
{
	u32 val;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	val = (__raw_readl(reg + RDMACTRL) & ~(RDMACTRL_IEN_MASK));
	if ((val & RDMACTRL_UPD_MASK) == 0U) {
		val |= ((u32)0x1U << RDMACTRL_UPD_SHIFT);
		__raw_writel(val, reg + RDMACTRL);
	}

	/* This code prevents fifo underrun in the example below:
	 * 1) PATH - R(1920x1080) - S(720x480) - W(720x480) - D(720x480)
	 * 2) Disable RDMA.
	 * 3) Unplug Scaler then fifo underrun is occurred Immediately.
	 * - Because the WMIX read size from RDMA even if RDMA was disabled
	 */
	__raw_writel(0x00000000, reg + RDMASIZE);
	val |= ((u32)0x1U << RDMACTRL_UPD_SHIFT);
	__raw_writel(val, reg + RDMACTRL);
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_RDMA_SetImageDisableNW);

void VIOC_RDMA_SetImageFormat(void __iomem *reg, unsigned int nFormat)
{
	u32 val;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	val = (__raw_readl(reg + RDMACTRL) & ~(RDMACTRL_FMT_MASK));
	val |= (nFormat << RDMACTRL_FMT_SHIFT);
	__raw_writel(val, reg + RDMACTRL);
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_RDMA_SetImageFormat);

void VIOC_RDMA_SetImageRGBSwapMode(
	void __iomem *reg, unsigned int rgb_mode)
{
	u32 val;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	val = (__raw_readl(reg + RDMACTRL) & ~(RDMACTRL_SWAP_MASK));
	val |= (rgb_mode << RDMACTRL_SWAP_SHIFT);
	__raw_writel(val, reg + RDMACTRL);
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_RDMA_SetImageRGBSwapMode);

void VIOC_RDMA_SetImageAlphaEnable(
	void __iomem *reg, unsigned int enable)
{
	u32 val;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	val = (__raw_readl(reg + RDMACTRL) & ~(RDMACTRL_AEN_MASK));
	val |= (enable << RDMACTRL_AEN_SHIFT);
	__raw_writel(val, reg + RDMACTRL);
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_RDMA_SetImageAlphaEnable);

void VIOC_RDMA_GetImageAlphaEnable(
	const void __iomem *reg, unsigned int *enable)
{
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	*enable =
		((__raw_readl(reg + RDMACTRL) & RDMACTRL_AEN_MASK)
		 >> RDMACTRL_AEN_SHIFT);
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_RDMA_GetImageAlphaEnable);

void VIOC_RDMA_SetImageAlphaSelect(
	void __iomem *reg, unsigned int select)
{
	u32 val;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	val = (__raw_readl(reg + RDMACTRL) & ~(RDMACTRL_ASEL_MASK));
	val |= (select << RDMACTRL_ASEL_SHIFT);
	__raw_writel(val, reg + RDMACTRL);
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_RDMA_SetImageAlphaSelect);

void VIOC_RDMA_SetImageY2RMode(
	void __iomem *reg, unsigned int y2r_mode)
{
	u32 val;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	val = (__raw_readl(reg + RDMACTRL) & ~(RDMACTRL_Y2RMD_MASK));
	val |= (((y2r_mode & 0x3U) << RDMACTRL_Y2RMD_SHIFT));
	__raw_writel(val, reg + RDMACTRL);
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_RDMA_SetImageY2RMode);

void VIOC_RDMA_SetImageY2REnable(
	void __iomem *reg, unsigned int enable)
{
	u32 val;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	val = (__raw_readl(reg + RDMACTRL) & ~(RDMACTRL_Y2R_MASK));
	val |= (enable << RDMACTRL_Y2R_SHIFT);
	__raw_writel(val, reg + RDMACTRL);
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_RDMA_SetImageY2REnable);

void VIOC_RDMA_SetImageR2YMode(
	void __iomem *reg, unsigned int r2y_mode)
{
	u32 val;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	val = (__raw_readl(reg + RDMACTRL) & ~(RDMACTRL_R2YMD_MASK));
	val |= (((r2y_mode & 0x3U) << RDMACTRL_R2YMD_SHIFT));
	__raw_writel(val, reg + RDMACTRL);
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_RDMA_SetImageR2YMode);

void VIOC_RDMA_SetImageR2YEnable(
	void __iomem *reg, unsigned int enable)
{
	u32 val;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	val = (__raw_readl(reg + RDMACTRL) & ~(RDMACTRL_R2Y_MASK));
	val |= (enable << RDMACTRL_R2Y_SHIFT);
	__raw_writel(val, reg + RDMACTRL);
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_RDMA_SetImageR2YEnable);

void VIOC_RDMA_GetImageR2YEnable(
	const void __iomem *reg, unsigned int *enable)
{
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	*enable =
		((__raw_readl(reg + RDMACTRL) & RDMACTRL_R2Y_MASK)
		 >> RDMACTRL_R2Y_SHIFT);
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_RDMA_GetImageR2YEnable);

void VIOC_RDMA_SetImageAlpha(
	void __iomem *reg, unsigned int nAlpha0, unsigned int nAlpha1)
{
	u32 val;

	val = ((nAlpha1 << RDMAALPHA_A1_SHIFT)
	       | (nAlpha0 << RDMAALPHA_A0_SHIFT));
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	__raw_writel(val, reg + RDMAALPHA);
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_RDMA_SetImageAlpha);

void VIOC_RDMA_GetImageAlpha(
	const void __iomem *reg, unsigned int *nAlpha0,
	unsigned int *nAlpha1)
{
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	*nAlpha1 =
		((__raw_readl(reg + RDMAALPHA) & RDMAALPHA_A1_MASK)
		 >> RDMAALPHA_A1_SHIFT);
	*nAlpha0 =
		((__raw_readl(reg + RDMAALPHA) & RDMAALPHA_A0_MASK)
		 >> RDMAALPHA_A0_SHIFT);
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_RDMA_GetImageAlpha);

void VIOC_RDMA_SetImageUVIEnable(
	void __iomem *reg, unsigned int enable)
{
	u32 val;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	val = (__raw_readl(reg + RDMACTRL) & ~(RDMACTRL_UVI_MASK));
	val |= (enable << RDMACTRL_UVI_SHIFT);
	__raw_writel(val, reg + RDMACTRL);
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_RDMA_SetImageUVIEnable);

void VIOC_RDMA_SetImage3DMode(void __iomem *reg, unsigned int mode_3d)
{
	u32 val;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	val = (__raw_readl(reg + RDMACTRL) & ~(RDMACTRL_3DM_MASK));
	val |= (mode_3d << RDMACTRL_3DM_SHIFT);
	__raw_writel(val, reg + RDMACTRL);
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_RDMA_SetImage3DMode);

void VIOC_RDMA_SetImageSize(
	void __iomem *reg, unsigned int sw, unsigned int sh)
{
	u32 val;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	val = ((sh << RDMASIZE_HEIGHT_SHIFT) | ((sw << RDMASIZE_WIDTH_SHIFT)));
	__raw_writel(val, reg + RDMASIZE);
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_RDMA_SetImageSize);

void VIOC_RDMA_GetImageSize(
	const void __iomem *reg, unsigned int *sw, unsigned int *sh)
{
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	*sw = ((__raw_readl(reg + RDMASIZE) & RDMASIZE_WIDTH_MASK)
	       >> RDMASIZE_WIDTH_SHIFT);
	*sh = ((__raw_readl(reg + RDMASIZE) & RDMASIZE_HEIGHT_MASK)
	       >> RDMASIZE_HEIGHT_SHIFT);
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_RDMA_GetImageSize);

void VIOC_RDMA_SetImageBase(
	void __iomem *reg, unsigned int nBase0, unsigned int nBase1,
	unsigned int nBase2)
{
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	__raw_writel(nBase0, reg + RDMABASE0);
	__raw_writel(nBase1, reg + RDMABASE1);
	__raw_writel(nBase2, reg + RDMABASE2);
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_RDMA_SetImageBase);

void VIOC_RDMA_SetImageRBase(
	void __iomem *reg, unsigned int nBase0, unsigned int nBase1,
	unsigned int nBase2)
{
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	__raw_writel(nBase0, reg + RDMA_RBASE0);
	__raw_writel(nBase1, reg + RDMA_RBASE1);
	__raw_writel(nBase2, reg + RDMA_RBASE2);
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_RDMA_SetImageRBase);

void VIOC_RDMA_SetImageOffset(
	void __iomem *reg, unsigned int imgFmt, unsigned int imgWidth)
{
	u32 val;
	unsigned int offset0 = 0, offset1 = 0;

	if (imgWidth > 0x1FFFU) {
		(void)pr_err("[ERR][RDMA] %s imgWidth(%d) is wrong\n", __func__, imgWidth);
		} else {
		switch (imgFmt) {
		case (unsigned int)TCC_LCDC_IMG_FMT_1BPP:
			// 1bpp indexed color
			offset0 = (1U * imgWidth) / 8U;
			break;
		case (unsigned int)TCC_LCDC_IMG_FMT_2BPP:
			// 2bpp indexed color
			offset0 = (1U * imgWidth) / 4U;
			break;
		case (unsigned int)TCC_LCDC_IMG_FMT_4BPP:
			// 4bpp indexed color
			offset0 = (1U * imgWidth) / 2U;
			break;
		case (unsigned int)TCC_LCDC_IMG_FMT_8BPP:
			// 8bpp indexed color
			offset0 = (1U * imgWidth);
			break;
		case (unsigned int)TCC_LCDC_IMG_FMT_RGB332:
			// RGB332 - 1bytes aligned -
						// R[7:5],G[4:2],B[1:0]
			offset0 = 1U * imgWidth;
			break;
		case (unsigned int)TCC_LCDC_IMG_FMT_RGB444:
			// RGB444 - 2bytes aligned -
						// A[15:12],R[11:8],G[7:3],B[3:0]
		case (unsigned int)TCC_LCDC_IMG_FMT_RGB565:
			// RGB565 - 2bytes aligned -
						// R[15:11],G[10:5],B[4:0]
		case (unsigned int)TCC_LCDC_IMG_FMT_RGB555:
			// RGB555 - 2bytes aligned -
						// A[15],R[14:10],G[9:5],B[4:0]
			offset0 = 2U * imgWidth;
			break;
		// case TCC_LCDC_IMG_FMT_RGB888:
		case (unsigned int)TCC_LCDC_IMG_FMT_RGB888:
			// RGB888 - 4bytes aligned -
						// A[31:24],R[23:16],G[15:8],B[7:0]
		case (unsigned int)TCC_LCDC_IMG_FMT_RGB666:
			// RGB666 - 4bytes aligned -
						// A[23:18],R[17:12],G[11:6],B[5:0]
			offset0 = 4U * imgWidth;
			break;
		case (unsigned int)TCC_LCDC_IMG_FMT_RGB888_3:
			// RGB888 - 3 bytes aligned :
						// B1[31:24],R0[23:16],G0[15:8],B0[7:0]
		case (unsigned int)TCC_LCDC_IMG_FMT_ARGB6666_3:
			// ARGB6666 - 3 bytes aligned :
						// A[23:18],R[17:12],G[11:6],B[5:0]
			offset0 = 3U * imgWidth;
			break;

		case (unsigned int)TCC_LCDC_IMG_FMT_444SEP:
			/* YUV444 or RGB444 Format */
			offset0 = imgWidth;
			offset1 = imgWidth;
			break;
		case (unsigned int)TCC_LCDC_IMG_FMT_YUV420SP:
			// YCbCr 4:2:0 Separated format - Not
						// Supported for Image 1 and 2
			offset0 = imgWidth;
			offset1 = imgWidth / 2U;
			break;
		case (unsigned int)TCC_LCDC_IMG_FMT_YUV422SP:
			// YCbCr 4:2:2 Separated format - Not
						// Supported for Image 1 and 2
			offset0 = imgWidth;
			offset1 = imgWidth / 2U;
			break;
		case (unsigned int)TCC_LCDC_IMG_FMT_UYVY:
			// YCbCr 4:2:2 Sequential format
		case (unsigned int)TCC_LCDC_IMG_FMT_VYUY:
			// YCbCr 4:2:2 Sequential format
		case (unsigned int)TCC_LCDC_IMG_FMT_YUYV:
			// YCbCr 4:2:2 Sequential format
		case (unsigned int)TCC_LCDC_IMG_FMT_YVYU:
			// YCbCr 4:2:2 Sequential format
			offset0 = 2U * imgWidth;
			break;
		case (unsigned int)TCC_LCDC_IMG_FMT_YUV420ITL0:
			// YCbCr 4:2:0 interleved type 0
			// format - Not Supported for Image 1
			// and 2
		case (unsigned int)TCC_LCDC_IMG_FMT_YUV420ITL1:
			// YCbCr 4:2:0 interleved type 1
			// format - Not Supported for Image 1
			// and 2
			offset0 = imgWidth;
			offset1 = imgWidth;
			break;
		case (unsigned int)TCC_LCDC_IMG_FMT_YUV422ITL0:
			// YCbCr 4:2:2 interleved type 0
			// format - Not Supported for Image 1
			// and 2
		case (unsigned int)TCC_LCDC_IMG_FMT_YUV422ITL1:
			// YCbCr 4:2:2 interleved type 1
			// format - Not Supported for Image 1
			// and 2
			offset0 = imgWidth;
			offset1 = imgWidth;
			break;
		case (unsigned int)TCC_LCDC_IMG_FMT_MAX:
		default:
			offset0 = imgWidth;
			offset1 = imgWidth;
			break;
		}

		val = ((offset1 << RDMAOFFS_OFFSET1_SHIFT)
			| (offset0 << RDMAOFFS_OFFSET0_SHIFT));
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		__raw_writel(val, reg + RDMAOFFS);
	}
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_RDMA_SetImageOffset);

void VIOC_RDMA_SetImageBfield(void __iomem *reg, unsigned int bfield)
{
	u32 val;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	val = (__raw_readl(reg + RDMACTRL) & ~(RDMACTRL_BF_MASK));
	val |= (bfield << RDMACTRL_BF_SHIFT);
	__raw_writel(val, reg + RDMACTRL);
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_RDMA_SetImageBfield);

void VIOC_RDMA_SetImageBFMD(void __iomem *reg, unsigned int bfmd)
{
	u32 val;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	val = (__raw_readl(reg + RDMACTRL) & ~(RDMACTRL_BFMD_MASK));
	val |= (bfmd << RDMACTRL_BFMD_SHIFT);
	__raw_writel(val, reg + RDMACTRL);
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_RDMA_SetImageBFMD);

void VIOC_RDMA_SetImageIntl(void __iomem *reg, unsigned int intl_en)
{
	u32 val;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	val = (__raw_readl(reg + RDMACTRL) & ~(RDMACTRL_INTL_MASK));
	val |= (intl_en << RDMACTRL_INTL_SHIFT);
	__raw_writel(val, reg + RDMACTRL);
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_RDMA_SetImageIntl);

/* set 1 : IREQ Masked( interrupt disable), set 0 : IREQ UnMasked( interrput
 * enable)
 */
void VIOC_RDMA_SetIreqMask(
	void __iomem *reg, unsigned int mask, unsigned int set)
{
	if (set == 0U) { /* Interrupt Enable*/
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		__raw_writel(~mask, reg + RDMAIRQMSK);
	} else { /* Interrupt Diable*/
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		__raw_writel(mask, reg + RDMAIRQMSK);
	}
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_RDMA_SetIreqMask);

/* STAT set : to clear status*/
void VIOC_RDMA_SetStatus(void __iomem *reg, unsigned int mask)
{
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	__raw_writel(mask, reg + RDMASTAT);
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_RDMA_SetStatus);

unsigned int VIOC_RDMA_GetStatus(const void __iomem *reg)
{
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	return __raw_readl(reg + RDMASTAT);
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_RDMA_GetStatus);

#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
void VIOC_RDMA_SetIssue(
	void __iomem *reg, unsigned int burst_length,
	unsigned int issue_cnt)
{
	u32 val;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	val = (__raw_readl(reg + RDMASCALE) & ~(RDMASCALE_ISSUE_MASK));
	val |= ((burst_length << 8U) | issue_cnt) << RDMASCALE_ISSUE_SHIFT;
	__raw_writel(val, reg + RDMASCALE);
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_RDMA_SetIssue);
#endif

void VIOC_RDMA_SetImageScale(
	void __iomem *reg, unsigned int scaleX, unsigned int scaleY)
{
	u32 val;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	val = (__raw_readl(reg + RDMASCALE))
		& ~(RDMASCALE_XSCALE_MASK | RDMASCALE_YSCALE_MASK);
	val |= ((scaleX << RDMASCALE_XSCALE_SHIFT)
		| (scaleY << RDMASCALE_YSCALE_SHIFT));
	__raw_writel(val, reg + RDMASCALE);
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_RDMA_SetImageScale);

void VIOC_RDMA_DUMP(const void __iomem *reg, unsigned int vioc_id)
{
	unsigned int cnt = 0;
	const void __iomem *pReg = reg;
	unsigned int Num = get_vioc_index(vioc_id);

	if (Num >= VIOC_RDMA_MAX) {
		/* Prevent KCS warning */
		(void)pr_err("[ERR][RDMA] %s Num:%d max num:%d\n", __func__, Num,
			VIOC_RDMA_MAX);
	} else {
		if (pReg == NULL) {
			pReg = VIOC_RDMA_GetAddress(vioc_id);
		}
		if (pReg != NULL) {
			(void)pr_info("[DBG][RDMA] RDMA-%d :: \n", Num);
			while (cnt < 0x50U) {
				/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
				(void)pr_info(
					"RDMA-%d + 0x%x: 0x%08x 0x%08x 0x%08x 0x%08x\n", Num, cnt,
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
EXPORT_SYMBOL(VIOC_RDMA_DUMP);

unsigned int VIOC_RDMA_Get_CAddress(const void __iomem *reg)
{
	unsigned int value = 0;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	value = __raw_readl(reg + RDMACADDR);

	return value;
}

void __iomem *VIOC_RDMA_GetAddress(unsigned int vioc_id)
{
	unsigned int Num = get_vioc_index(vioc_id);
	void __iomem *ret = NULL;

	if ((Num >= VIOC_RDMA_MAX) || (pRDMA_reg[Num] == NULL)) {
		(void)pr_err("[ERR][RDMA] %s Num:%d max num:%d\n", __func__, Num,
			VIOC_RDMA_MAX);
		ret = NULL;
	} else {
		ret = pRDMA_reg[Num];
	}

	return ret;
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_RDMA_GetAddress);

int VIOC_RDMA_IsVRDMA(unsigned int vioc_id)
{
	unsigned int Num = get_vioc_index(vioc_id);
	int ret = 0;

	if (Num >= VIOC_RDMA_MAX) {
		/* Prevent KCS warning */
		ret = 0;
	} else {
		ret = VRDMAS[Num];
	}

	return ret;
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_RDMA_IsVRDMA);

int VIOC_RDMA_GetImageNum(unsigned int vioc_id)
{
	unsigned int Num = get_vioc_index(vioc_id);
	int ret = -1;

	if ((Num >= VIOC_RDMA_MAX) || (IMAGE_NUM[Num] == -1)) {
		(void)pr_err("[ERR][RDMA] %s Num:%d max num:%d\n", __func__, Num,
	       VIOC_RDMA_MAX);
		ret = -1;
	} else {
		ret = IMAGE_NUM[Num];
	}

	return ret;
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_RDMA_GetImageNum);

static int __init vioc_rdma_init(void)
{
	unsigned int i;

	ViocRdma_np =
		of_find_compatible_node(NULL, NULL, "telechips,vioc_rdma");
	if (ViocRdma_np == NULL) {
		/* Prevent KCS warning */
		(void)pr_info("[INF][RDMA] disabled\n");
	} else {
		for (i = 0; i < VIOC_RDMA_MAX; i++) {
			pRDMA_reg[i] = (void __iomem *)of_iomap(
				ViocRdma_np,
				(is_VIOC_REMAP != 0U) ? ((int)i + (int)VIOC_RDMA_MAX) : (int)i);

			if (pRDMA_reg[i] != NULL) {
				/* Prevent KCS warning */
				(void)pr_info("[INF][RDMA] vioc-rdma-%d\n", i);
			}
		}
	}
	return 0;
}
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
arch_initcall(vioc_rdma_init);
