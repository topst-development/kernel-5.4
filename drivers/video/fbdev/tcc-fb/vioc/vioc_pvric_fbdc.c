/*
 * vioc_pvric_fbdc.c
 * Author:  <linux@telechips.com>
 * Created:  10, 2020
 * Description: TCC VIOC h/w block
 *
 * Copyright (C) 2020 Telechips
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
#include <video/tcc/tccfb_ioctrl.h>
#include <video/tcc/vioc_pvric_fbdc.h>

#define PVRIC_FBDC_MAX_N 2U

static struct device_node *pViocPVRICFBDC_np;
static void __iomem *pPVRICFBDC_reg[PVRIC_FBDC_MAX_N] = { 0 };

void VIOC_PVRIC_FBDC_SetARGBSwizzMode(void __iomem *reg,
				      VIOC_PVRICCTRL_SWIZZ_MODE mode)
{
	unsigned int val;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	val = (__raw_readl(reg + PVRICCTRL) & ~(PVRICCTRL_SWIZZ_MASK));
	val |= ((unsigned int)mode << PVRICCTRL_SWIZZ_SHIFT);
	__raw_writel(val, reg + PVRICCTRL);
}

void VIOC_PVRIC_FBDC_SetUpdateInfo(void __iomem *reg,
				   unsigned int enable)
{
	unsigned int val;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	val = (__raw_readl(reg + PVRICCTRL) & ~(PVRICCTRL_UPD_MASK));
	val |= (enable << PVRICCTRL_UPD_SHIFT);
	__raw_writel(val, reg + PVRICCTRL);
}

void VIOC_PVRIC_FBDC_SetFormat(void __iomem *reg,
			       VIOC_PVRICCTRL_FMT_MODE fmt)
{
	unsigned int val;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	val = (__raw_readl(reg + PVRICCTRL) & ~(PVRICCTRL_FMT_MASK));
	val |= ((unsigned int)fmt << PVRICCTRL_FMT_SHIFT);
	__raw_writel(val, reg + PVRICCTRL);
}

void VIOC_PVRIC_FBDC_SetTileType(void __iomem *reg,
				 VIOC_PVRICCTRL_TILE_TYPE type)
{
	unsigned int val;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	val = (__raw_readl(reg + PVRICCTRL) & ~(PVRICCTRL_TILE_TYPE_MASK));
	val |= ((unsigned int)type << PVRICCTRL_TILE_TYPE_SHIFT);
	__raw_writel(val, reg + PVRICCTRL);
}

void VIOC_PVRIC_FBDC_SetLossyDecomp(void __iomem *reg,
				    unsigned int enable)
{
	unsigned int val;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	val = (__raw_readl(reg + PVRICCTRL) & ~(PVRICCTRL_LOSSY_MASK));
	val |= (enable << PVRICCTRL_LOSSY_SHIFT);
	__raw_writel(val, reg + PVRICCTRL);
}

void VIOC_PVRIC_FBDC_SetFrameSize(void __iomem *reg,
				  unsigned int width, unsigned int height)
{
	unsigned int val;

	val = (((height & 0x1FFFU) << PVRICSIZE_HEIGHT_SHIFT) |
	       ((width & 0x1FFFU) << PVRICSIZE_WIDTH_SHIFT));
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	__raw_writel(val, reg + PVRICSIZE);
}

void VIOC_PVRIC_FBDC_GetFrameSize(const void __iomem *reg,
				  unsigned int *width, unsigned int *height)
{
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	*width = ((__raw_readl(reg + PVRICSIZE) &
		   PVRICSIZE_WIDTH_MASK) >> PVRICSIZE_WIDTH_SHIFT);
	*height = ((__raw_readl(reg + PVRICSIZE) &
		    PVRICSIZE_HEIGHT_MASK) >> PVRICSIZE_HEIGHT_SHIFT);
}

void VIOC_PVRIC_FBDC_SetRequestBase(void __iomem *reg,
				    unsigned int base)
{
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	__raw_writel((base & 0xFFFFFFFFU), reg + PVRICRADDR);
}

void VIOC_PVRIC_FBDC_GetCurTileNum(const void __iomem *reg,
				   unsigned int *tile_num)
{
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	*tile_num = ((__raw_readl(reg + PVRICURTILE) &
		      PVRICCURTILE_MASK) >> PVRICCURTILE_SHIFT);
}

void VIOC_PVRIC_FBDC_SetOutBufOffset(void __iomem *reg,
				     unsigned int imgFmt, unsigned int imgWidth)
{
	unsigned int offset;

	if (imgWidth > 0x1FFFU) {
		(void)pr_err("[ERR] %s imgWidth(%d) is wrong\n", __func__, imgWidth);
	} else {
		switch (imgFmt) {
		case (unsigned int)TCC_LCDC_IMG_FMT_RGB888:	// RGB888 - 4bytes aligned -
			offset = 4U * imgWidth;
			break;
		case (unsigned int)TCC_LCDC_IMG_FMT_RGB888_3:	// RGB888 - 3 bytes aligned :
			offset = 3U * imgWidth;
			break;
		default:
			(void)pr_info("%s %d imgFormat is not supported.\n", __func__,
				imgFmt);
			offset = 4U * imgWidth;
			break;
		}
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		__raw_writel((offset & 0xFFFFFFFFU), reg + PVRICOFFS);
	}
}

void VIOC_PVRIC_FBDC_SetOutBufBase(void __iomem *reg,
				   unsigned int base)
{
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	__raw_writel((base & 0xFFFFFFFFU), reg + PVRICOADDR);
}

void vioc_pvric_fbdc_set_val0_cr_ch0123(void __iomem *reg,
				  unsigned int ch0, unsigned int ch1,
				  unsigned int ch2, unsigned int ch3)
{
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	__raw_writel((ch3 << PVRICCCR_CH3_PIXEL_SHIFT) |
				(ch2 << PVRICCCR_CH2_PIXEL_SHIFT) |
				(ch1 << PVRICCCR_CH1_PIXEL_SHIFT) |
				/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
				(ch0 << PVRICCCR_CH0_PIXEL_SHIFT), reg + PVRICCCR0);
}

void vioc_pvric_fbdc_set_val1_cr_ch0123(void __iomem *reg,
				  unsigned int ch0, unsigned int ch1,
				  unsigned int ch2, unsigned int ch3)
{
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	__raw_writel((ch3 << PVRICCCR_CH3_PIXEL_SHIFT) |
			(ch2 << PVRICCCR_CH2_PIXEL_SHIFT) |
			(ch1 << PVRICCCR_CH1_PIXEL_SHIFT) |
			/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
			(ch0 << PVRICCCR_CH0_PIXEL_SHIFT), reg + PVRICCCR1);
}

void vioc_pvric_fbdc_set_cr_filter_enable(void __iomem *reg)
{
	unsigned int val;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	val = (__raw_readl(reg + PVRICFSTAUS) &
	  ~(PVRICFSTAUS_CRF_ENABLE_SHIFT));
	val |= ((u32)1U << PVRICFSTAUS_CRF_ENABLE_SHIFT);
	__raw_writel(val, reg + PVRICFSTAUS);
}

void vioc_pvric_fbdc_set_cr_filter_disable(void __iomem *reg)
{
	unsigned int val;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	val = (__raw_readl(reg + PVRICFSTAUS) &
	  ~(PVRICFSTAUS_CRF_ENABLE_SHIFT));
	__raw_writel(val, reg + PVRICFSTAUS);
}

void VIOC_PVRIC_FBDC_SetIrqMask(void __iomem *reg,
				unsigned int enable, unsigned int mask)
{
	if (enable != 0U) {
		/* Interrupt Enable */
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		__raw_writel((~mask << PVRICSTS_IRQEN_IDLE_SHIFT),
			reg + PVRICSTS);
	} else {
		/* Interrupt Diable */
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		__raw_writel((mask << PVRICSTS_IRQEN_IDLE_SHIFT),
			reg + PVRICSTS);
	}
}

unsigned int VIOC_PVRIC_FBDC_GetIdle(const void __iomem *reg)
{
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	return __raw_readl(reg + PVRICIDLE);
}

unsigned int VIOC_PVRIC_FBDC_GetStatus(const void __iomem *reg)
{
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	return (__raw_readl(reg + PVRICSTS)) & PVRICSYS_IRQ_ALL;
}

void VIOC_PVRIC_FBDC_ClearIrq(void __iomem *reg, unsigned int mask)
{
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	__raw_writel(mask, reg + PVRICSTS);
}

void VIOC_PVRIC_FBDC_TurnOn(void __iomem *reg)
{
	unsigned int val;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	val = (__raw_readl(reg + PVRICCTRL) & ~(PVRICCTRL_START_MASK));
	val |= PVRICCTRL_START_MASK;
	__raw_writel(val, reg + PVRICCTRL);
}

void VIOC_PVRIC_FBDC_TurnOFF(void __iomem *reg)
{
	unsigned int val;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	val = (__raw_readl(reg + PVRICCTRL) & ~(PVRICCTRL_START_MASK));
	__raw_writel(val, reg + PVRICCTRL);
	VIOC_PVRIC_FBDC_SetUpdateInfo(reg, 1U);
}

unsigned int VIOC_PVRIC_FBDC_isTurnOnOff(const void __iomem *reg)
{
	unsigned int val;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	val = (__raw_readl(reg + PVRICCTRL) & (PVRICCTRL_START_MASK))
		>> PVRICCTRL_START_SHIFT;

	return val;
}

int VIOC_PVRIC_FBDC_SetBasicConfiguration(void __iomem *reg,
					  unsigned int base,
					  unsigned int imgFmt,
					  unsigned int imgWidth,
					  unsigned int imgHeight,
					  unsigned int decomp_mode)
{
	int ret = 0;

	//pr_info("%s- Start\n", __func__);
	if (imgFmt == VIOC_IMG_FMT_RGB888) {
		VIOC_PVRIC_FBDC_SetFormat(reg, PVRICCTRL_FMT_RGB888);
	}
	else if (imgFmt == VIOC_IMG_FMT_ARGB8888) {
		VIOC_PVRIC_FBDC_SetFormat(reg, PVRICCTRL_FMT_ARGB8888);
		VIOC_PVRIC_FBDC_SetARGBSwizzMode(reg,
						 VIOC_PVRICCTRL_SWIZZ_ARGB);
	}
	else {
		(void)pr_err("%s imgFmt:%d Not support condition\n", __func__,
		       imgFmt);
		ret = -1;
	}

	if (imgWidth <= (unsigned int)VIOC_PVRICSIZE_MAX_WIDTH_8X8) {
		VIOC_PVRIC_FBDC_SetTileType(reg, VIOC_PVRICCTRL_TILE_TYPE_8X8);
	}
	else if (imgWidth <= (unsigned int)VIOC_PVRICSIZE_MAX_WIDTH_16X4) {
		VIOC_PVRIC_FBDC_SetTileType(reg, VIOC_PVRICCTRL_TILE_TYPE_16X4);
	}
	else if (imgWidth <= (unsigned int)VIOC_PVRICSIZE_MAX_WIDTH_32X2) {
		VIOC_PVRIC_FBDC_SetTileType(reg, VIOC_PVRICCTRL_TILE_TYPE_32X2);
	}
	else {
		(void)pr_err("%s imgWidth:%d Not suport condition\n", __func__,
		       imgWidth);
		ret = -1;
	}
	VIOC_PVRIC_FBDC_SetLossyDecomp(reg, decomp_mode);

	VIOC_PVRIC_FBDC_SetFrameSize(reg, imgWidth, imgHeight);
	VIOC_PVRIC_FBDC_SetOutBufOffset(reg, imgFmt, imgWidth);
	VIOC_PVRIC_FBDC_SetOutBufBase(reg, base);

	//pr_info("%s - End\n", __func__);
	return ret;
}

void VIOC_PVRIC_FBDC_DUMP(const void __iomem *reg, unsigned int vioc_id)
{
	unsigned int cnt = 0;
	const void __iomem *pReg = reg;
	unsigned int Num = get_vioc_index(vioc_id);

	if (pReg == NULL) {
		pReg = VIOC_PVRIC_FBDC_GetAddress(vioc_id);
	}

	if (Num < PVRIC_FBDC_MAX_N) {
		(void)pr_info("[DBG][FBDC] PVRIC-FBDC-%d :: \n", Num);

		while (cnt < 0x100U) {
			/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
			(void)pr_info("[DBG][FBDC] PVRIC-FBDC-%d + 0x%x: 0x%08x 0x%08x 0x%08x 0x%08x\n",
				 Num, cnt, __raw_readl(pReg + cnt),
				 __raw_readl(pReg + cnt + 0x4U),
				 __raw_readl(pReg + cnt + 0x8U),
				 __raw_readl(pReg + cnt + 0xCU));
			cnt += 0x10U;
		}
	}
	else {
		(void)pr_err("[ERR][FBDC] err %s Num:%d , max :%d\n", __func__, Num,
			PVRIC_FBDC_MAX_N);
	}
}

void __iomem *VIOC_PVRIC_FBDC_GetAddress(unsigned int vioc_id)
{
	unsigned int Num = get_vioc_index(vioc_id);
	void __iomem *ret;

	if (Num < PVRIC_FBDC_MAX_N) {
		if (pPVRICFBDC_reg[Num] == NULL) {
			(void)pr_err("[ERR][FBDC] %s pPVRICFBDC_reg:%p\n", __func__,
				pPVRICFBDC_reg[Num]);
		}
		ret = pPVRICFBDC_reg[Num];
	}
	else {
		(void)pr_err("[ERR][FBDC] err %s Num:%d , max :%d\n", __func__, Num,
			PVRIC_FBDC_MAX_N);
		ret = NULL;
	}

	return ret;
}

static int __init vioc_pvric_fbdc_init(void)
{
	unsigned int i;

	pViocPVRICFBDC_np =
	    of_find_compatible_node(NULL, NULL, "telechips,vioc_pvric_fbdc");

	if (pViocPVRICFBDC_np == NULL) {
		/* Prevent KCS warning */
		(void)pr_info("[INF][FBDC] vioc-pvric_fbdc: disabled\n");
	} else {
		for (i = 0; i < PVRIC_FBDC_MAX_N; i++) {
			pPVRICFBDC_reg[i] =
				(void __iomem *)of_iomap(pViocPVRICFBDC_np,
				(is_VIOC_REMAP != 0U) ? ((int)i + (int)PVRIC_FBDC_MAX_N) : (int)i);

			if (pPVRICFBDC_reg[i] != NULL) {
				(void)pr_info("[INF][FBDC] vioc-pvric_fbdc%d\n",
					i);
			}
		}
	}
	return 0;
}
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
arch_initcall(vioc_pvric_fbdc_init);
