// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * linux/arch/arm/mach-tcc893x/vioc_rdma.c
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
#include <linux/kernel.h>
#include <linux/io.h>

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_dma.h>
#include <soc/tcc/chipinfo.h>

#include <video/tcc/vioc_global.h>
#include <video/tcc/tcc_types.h>
#include <video/tcc/vioc_ddicfg.h> // is_VIOC_REMAP
#include <video/tcc/vioc_mc.h>
#if defined(DOLBY_VISION_CHECK_SEQUENCE)
#include <video/tcc/vioc_v_dv.h>
#endif

#define MC_MAX_N 2U

static struct device_node *ViocMC_np;
static void __iomem *pMC_reg[MC_MAX_N] = {0};

void VIOC_MC_Get_OnOff(const void __iomem *reg, uint *enable)
{
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	*enable =
		((__raw_readl(reg + MC_CTRL) & MC_CTRL_START_MASK)
		 >> MC_CTRL_START_SHIFT);
}

void VIOC_MC_Start_OnOff(void __iomem *reg, uint OnOff)
{
	u32 val;

#if defined(DOLBY_VISION_CHECK_SEQUENCE)
	VIOC_MC_Get_OnOff(reg, &val);
	if (val != OnOff) {
		dprintk_dv_sequence(
			"### ====> %d MC %s\n",
			(reg == VIOC_MC_GetAddress(0)) ? 0 : 1,
			OnOff ? "On" : "Off");
	}
#endif

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	val = (__raw_readl(reg + MC_CTRL) & ~(MC_CTRL_START_MASK));
	val |= ((OnOff & 0x1U) << MC_CTRL_START_SHIFT);
	val |= ((u32)0x1U << MC_CTRL_UPD_SHIFT);
	__raw_writel(val, reg + MC_CTRL);
}

void VIOC_MC_UPD(void __iomem *reg)
{
	u32 val;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	if ((__raw_readl(reg + MC_CTRL) & MC_CTRL_UPD_MASK) != 0U) {
		/* Prevent KCS warning */
	} else {
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		val = (__raw_readl(reg + MC_CTRL) & ~(MC_CTRL_UPD_MASK));
		val |= ((u32)0x1U << MC_CTRL_UPD_SHIFT);
		__raw_writel(val, reg + MC_CTRL);
	}
}

void VIOC_MC_Y2R_OnOff(void __iomem *reg, uint OnOff, uint y2rmode)
{
	u32 val;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	val = (__raw_readl(reg + MC_CTRL)
	       & ~(MC_CTRL_Y2REN_MASK | MC_CTRL_Y2RMD_MASK));
	val |= (((OnOff & 0x1U) << MC_CTRL_Y2REN_SHIFT)
		| ((y2rmode & 0x7U) << MC_CTRL_Y2RMD_SHIFT));
	__raw_writel(val, reg + MC_CTRL);
}

void VIOC_MC_Start_BitDepth(void __iomem *reg, uint Chroma, uint Luma)
{
	u32 val;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	val = (__raw_readl(reg + MC_CTRL)
	       & ~(MC_CTRL_DEPC_MASK | MC_CTRL_DEPY_MASK));
	val |= (((Chroma & 0x3U) << MC_CTRL_DEPC_SHIFT)
		| ((Luma & 0x3U) << MC_CTRL_DEPY_SHIFT));
	__raw_writel(val, reg + MC_CTRL);
}

void VIOC_MC_OFFSET_BASE(void __iomem *reg, uint base_y, uint base_c)
{
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	__raw_writel(
		((base_y & 0xFFFFFFFFU) << MC_OFS_BASE_Y_BASE_SHIFT),
		reg + MC_OFS_BASE_Y);
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	__raw_writel(
		((base_c & 0xFFFFFFFFU) << MC_OFS_BASE_C_BASE_SHIFT),
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		reg + MC_OFS_BASE_C);
}

void VIOC_MC_FRM_BASE(void __iomem *reg, uint base_y, uint base_c)
{
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	__raw_writel(
		((base_y & 0xFFFFFFFFU) << MC_FRM_BASE_Y_BASE_SHIFT),
		reg + MC_FRM_BASE_Y);
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	__raw_writel(
		((base_c & 0xFFFFFFFFU) << MC_FRM_BASE_C_BASE_SHIFT),
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		reg + MC_FRM_BASE_C);
}

void VIOC_MC_FRM_SIZE(void __iomem *reg, uint xsize, uint ysize)
{
	u32 val;

	val = (((ysize & 0x3FFFU) << MC_FRM_SIZE_YSIZE_SHIFT)
	       | ((xsize & 0x3FFFU) << MC_FRM_SIZE_XSIZE_SHIFT));
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	__raw_writel(val, reg + MC_FRM_SIZE);
}

void VIOC_MC_FRM_SIZE_MISC(
	void __iomem *reg, uint pic_height, uint stride_y,
	uint stride_c)
{
	u32 val;

	val = ((pic_height & 0x3FFFU) << MC_PIC_HEIGHT_SHIFT);
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	__raw_writel(val, reg + MC_PIC);

	val = (((stride_c & 0xFFFFU) << MC_FRM_STRIDE_STRIDE_C_SHIFT)
	       | ((stride_y & 0xFFFFU) << MC_FRM_STRIDE_STRIDE_Y_SHIFT));
	__raw_writel(val, reg + MC_FRM_STRIDE);
}

void VIOC_MC_FRM_POS(void __iomem *reg, uint xpos, uint ypos)
{
	u32 val;

	val = (((ypos & 0x1FFFU) << MC_FRM_POS_YPOS_SHIFT)
	       | ((xpos & 0x1FFFU) << MC_FRM_POS_XPOS_SHIFT));
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	__raw_writel(val, reg + MC_FRM_POS);
}

void VIOC_MC_ENDIAN(
	void __iomem *reg, uint ofs_endian, uint comp_endian)
{
	u32 val;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	val = (__raw_readl(reg + MC_FRM_MISC0)
	       & ~(MC_FRM_MISC0_COMP_ENDIAN_MASK
		   | MC_FRM_MISC0_OFS_ENDIAN_MASK));
	val |= (((comp_endian & 0xFU) << MC_FRM_MISC0_COMP_ENDIAN_SHIFT)
		| ((ofs_endian & 0xFU) << MC_FRM_MISC0_OFS_ENDIAN_SHIFT));
	__raw_writel(val, reg + MC_FRM_MISC0);
}

void VIOC_MC_DITH_CONT(void __iomem *reg, uint en, uint sel)
{
	u32 val;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	val = (__raw_readl(reg + MC_DITH_CTRL)
	       & ~(MC_DITH_CTRL_EN_MASK | MC_DITH_CTRL_SEL_MASK));
	val |= (((sel & 0xFU) << MC_DITH_CTRL_SEL_SHIFT)
	       | ((en & 0xFFU) << MC_DITH_CTRL_EN_SHIFT));
	__raw_writel(val, reg + MC_DITH_CTRL);
}

#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
void VIOC_MC_SetDefaultAlpha(void __iomem *reg, uint alpha)
{
	u32 val;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	val = (__raw_readl(reg + MC_FRM_MISC1) & ~(MC_FRM_MISC1_ALPHA_MASK));
	val |= ((alpha & 0xFFU) << MC_FRM_MISC1_ALPHA_SHIFT);
	__raw_writel(val, reg + MC_FRM_MISC1);
#else
	val = (__raw_readl(reg + MC_TIMEOUT) & ~(MC_TIMEOUT_ALPHA_MASK));
	val |= ((alpha & 0x3FFU) << MC_TIMEOUT_ALPHA_SHIFT);
	__raw_writel(val, reg + MC_TIMEOUT);
#endif
}

void __iomem *VIOC_MC_GetAddress(unsigned int vioc_id)
{
	unsigned int Num = get_vioc_index(vioc_id);
	void __iomem *ret = NULL;

	if ((Num >= VIOC_MC_MAX) || (pMC_reg[Num] == NULL)) {
		(void)pr_err("[ERR][MC] %s Num:%d max:%d\n", __func__, Num, VIOC_MC_MAX);
		ret = NULL;
	} else {
		ret = pMC_reg[Num];
	}

	return ret;
}

int tvc_mc_get_info(unsigned int component_num, mc_info_type *pMC_info)
{
	const void __iomem *reg;
	unsigned int value = 0;
	int ret = -1;

	if ((component_num >= VIOC_MC0)
	    && (component_num <= (VIOC_MC0 + VIOC_MC_MAX))) {
		reg = VIOC_MC_GetAddress(component_num);
		if (reg == NULL) {
			/* Prevent KCS warning */
			ret = -1;
		} else {
			/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
			value = __raw_readl(reg + MC_FRM_SIZE);
			pMC_info->width = (unsigned short)((value & MC_FRM_SIZE_XSIZE_MASK)
				>> MC_FRM_SIZE_XSIZE_SHIFT);
			pMC_info->height = (unsigned short)((value & MC_FRM_SIZE_YSIZE_MASK)
				>> MC_FRM_SIZE_YSIZE_SHIFT);
			ret = 0;
		}
	} else {
		ret = -1;
	}
	return ret;
}

void VIOC_MC_DUMP(unsigned int vioc_id)
{
	const void __iomem *pReg;
	unsigned int cnt = 0;
	unsigned int Num = get_vioc_index(vioc_id);

	if (Num >= VIOC_MC_MAX) {
		/* Prevent KCS warning */
		(void)pr_err("[ERR][MC] %s Num:%d max:%d\n", __func__, Num, VIOC_MC_MAX);
	} else {
		pReg = VIOC_MC_GetAddress(Num);

		(void)pr_info("[DBG][MC] MC ::\n");
		while (cnt < 0x100U) {
			/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
			(void)pr_info(
				"[DBG][MC] MC -  + 0x%x: 0x%08x 0x%08x 0x%08x 0x%08x\n",
				cnt, __raw_readl(pReg + cnt),
				__raw_readl(pReg + cnt + 0x4U),
				__raw_readl(pReg + cnt + 0x8U),
				__raw_readl(pReg + cnt + 0xCU));
			cnt += 0x10U;
		}
	}
}

static int __init vioc_mc_init(void)
{
	unsigned int i;

	ViocMC_np = of_find_compatible_node(NULL, NULL, "telechips,vioc_mc");
	if (ViocMC_np == NULL) {
		/* Prevent KCS warning */
		(void)pr_info("[INF][MC] disabled\n");
	} else {
		for (i = 0; i < MC_MAX_N; i++) {
			pMC_reg[i] = (void __iomem *)of_iomap(
				ViocMC_np, (is_VIOC_REMAP != 0U) ? ((int)i + (int)MC_MAX_N) : (int)i);

			if (pMC_reg[i] != NULL) {
				/* Prevent KCS warning */
				(void)pr_info("[INF][MC] vioc-mc%d\n", i);
			}
		}
	}
	return 0;
}
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
arch_initcall(vioc_mc_init);
