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
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/of_address.h>
#include <linux/types.h>

#include <video/tcc/vioc_global.h>
#include <video/tcc/tcc_types.h>
#include <video/tcc/vioc_ddicfg.h>	// is_VIOC_REMAP
#include <video/tcc/vioc_lut.h>

static void __iomem *pLUT_reg;

#define REG_VIOC_LUT(offset) (pLUT_reg + (offset))
#define LUT_CTRL_R REG_VIOC_LUT(0U)
#define LUT_TABLE_R REG_VIOC_LUT(0x400U)

// LUT VIOCk Config
#define L_CONFIG_SEL_SHIFT 0U
#define L_CONFIG_SEL_MASK (0xFFU << L_CONFIG_SEL_SHIFT)

#define L_CFG_EN_SHIFT 31U
#define L_CFG_EN_MASK ((u32)0x1U << L_CFG_EN_SHIFT)

// vioc lut config register write & read
#define lut_writel __raw_writel
#define lut_readl __raw_readl


void __iomem *lut_get_address(unsigned int lut_n, int *is_dev)
{
	void __iomem *reg;

	switch (get_vioc_index(lut_n)) {
	case 0:
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		reg = (void __iomem *)REG_VIOC_LUT(VIOC_LUT_DEV0_OFFSET);
		*is_dev = 1;
		break;
	case 1:
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		reg = (void __iomem *)REG_VIOC_LUT(VIOC_LUT_DEV1_OFFSET);
		*is_dev = 1;
		break;
	case 2:
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		reg = (void __iomem *)REG_VIOC_LUT(VIOC_LUT_DEV2_OFFSET);
		*is_dev = 1;
		break;
	case 3:
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		reg = (void __iomem *)REG_VIOC_LUT(VIOC_LUT_COMP0_OFFSET);
		*is_dev = 0;
		break;
	case 4:
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		reg = (void __iomem *)REG_VIOC_LUT(VIOC_LUT_COMP1_OFFSET);
		*is_dev = 0;
		break;
	#if defined(CONFIG_ARCH_TCC805X)
	case 7:
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		reg = (void __iomem *)REG_VIOC_LUT(VIOC_LUT_DEV3_OFFSET);
		*is_dev = 1;
		break;
	#endif
	case 5:
		// reg = (void __iomem *)REG_VIOC_LUT(VIOC_LUT_COMP2_OFFSET);
		// *is_dev = 0;
		//break;
	case 6:
		// reg = (void __iomem *)REG_VIOC_LUT(VIOC_LUT_COMP3_OFFSET);
		// *is_dev = 0;
		// break;
	default:
		(void)pr_err("[ERR][LUT] %s lut number 0x%x is out of range\n",
			__func__, lut_n);
		reg = NULL;
		break;
	}

	return reg;
}

int lut_get_pluginComponent_index(unsigned int tvc_n)
{
	int ret = -1;
	unsigned int temp = 0U; /* avoid MISRA C-2012 Rule 10.8 */

	switch (get_vioc_type(tvc_n)) {
	case get_vioc_type(VIOC_RDMA):
		switch (get_vioc_index(tvc_n)) {
		case 16:
			(void)pr_info(" >>plugin to rdma16\n");
			ret = 17;
			break;
		case 17:
			(void)pr_info(" >>plugin to rdma17\n");
			ret = 19;
			break;
		default:
			(void)pr_info(" >>plugin to rdma%02d\n",
				 get_vioc_index(tvc_n));
			/* avoid MISRA C-2012 Rule 10.8 */
			temp = get_vioc_index(tvc_n);
			ret = (int)temp;
			break;
		}
		break;
	case get_vioc_type(VIOC_VIN):
		switch (get_vioc_index(tvc_n)) {
		case 0:
			(void)pr_info(" >>plugin to vin0\n");
			ret = 16;
			break;
		case 1:
			(void)pr_info(" >>plugin to vin1\n");
			ret = 18;
			break;
		default:
			(void)pr_err("%s component is wrong. type(%d) index(%d)\n",
			__func__, get_vioc_type(tvc_n), get_vioc_index(tvc_n));
			ret = -1;
			break;
		}
		break;
	case get_vioc_type(VIOC_WDMA):
		/* avoid MISRA C-2012 Rule 10.8 */
		temp = (20U + (get_vioc_index(tvc_n)));
		ret = (int)temp;
		break;
	default:
		(void)pr_err("%s component type(%d) is wrong. \n",
			__func__, get_vioc_type(tvc_n));
		ret = -1;
		break;
	}

	return ret;
}

int lut_get_Component_index_to_tvc(unsigned int plugin_n)
{
	int ret = -1;

	if (plugin_n <= 0xfU) {
		/* Prevent KCS warning */
		ret = (int)VIOC_RDMA00 + (int)plugin_n;
	} else if (plugin_n == 0x10U) {
		/* Prevent KCS warning */
		ret = (int)VIOC_VIN00;
	} else if (plugin_n == 0x11U) {
		/* Prevent KCS warning */
		ret = (int)VIOC_RDMA16;
#if !defined(CONFIG_ARCH_TCC897X)	//tt
	} else if (plugin_n == 0x12U) {
		/* Prevent KCS warning */
		ret = (int)VIOC_VIN10;
	} else if (plugin_n == 0x13U) {
		/* Prevent KCS warning */
		ret = (int)VIOC_RDMA17;
#endif
	} else if (plugin_n <= 0x1CU) {
		/* Prevent KCS warning */
		ret = ((int)VIOC_WDMA00 + ((int)plugin_n - 0x14));
	} else {
		/* Prevent KCS warning */
		ret = -1;
	}

	return ret;
}

// R, G, B is 10bit color format and R = Y, G = U, B = V
void tcc_set_lut_table_to_color(unsigned int lut_n,
				unsigned int R, unsigned int G, unsigned int B)
{
	unsigned int i, reg_off, lut_index;
	#if defined(LUT_TABLE_IND_R)
	unsigned int ind_v;
	#endif
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	void __iomem *table_reg = (void __iomem *)LUT_TABLE_R;
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	void __iomem *ctrl_reg = (void __iomem *)LUT_CTRL_R;

	unsigned int color = 0;

	color = ((R & 0x3FFU) << 20U) | ((G & 0x3FFU) << 10U) | (B & 0x3FFU);
	(void)pr_info("%s R:0x%x, G:0x%x B:0x%x, color:0x%x\n",
		__func__, R, G, B, color);

	/* Get Component index */
	lut_index = get_vioc_index(lut_n);
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	lut_writel(lut_index, ctrl_reg);

	// lut table setting
	for (i = 0; i < LUT_TABLE_SIZE; i++) {
		#if defined(LUT_TABLE_IND_R)
		ind_v = i >> 8U;
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		lut_writel(ind_v, LUT_TABLE_IND_R);
		#endif
		reg_off = (0xFFU & i);
		/* avoid CERT-C Integers Rule INT30-C */
		if (reg_off < (UINT_MAX / 4U)) {
			/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
			lut_writel(color, table_reg + (reg_off * 0x4U));
		}
	}

	#if defined(LUT_UPDATE_PEND)
	if (lut_index >= get_vioc_index(VIOC_LUT_COMP0)) {
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		lut_writel((u32)1U << (lut_index - get_vioc_index(VIOC_LUT_COMP0)) |
			   lut_readl(LUT_UPDATE_PEND), LUT_UPDATE_PEND);
	}
	#endif
}

void tcc_set_lut_table(unsigned int lut_n, const unsigned int *table)
{
	unsigned int i, reg_off, lut_index;
	#if defined(LUT_TABLE_IND_R)
	unsigned int ind_v;
	#endif
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	void __iomem *table_reg = (void __iomem *)LUT_TABLE_R;
	void __iomem *ctrl_reg = (void __iomem *)LUT_CTRL_R;

	// lut table select
	lut_index = get_vioc_index(lut_n);
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	lut_writel(lut_index, ctrl_reg);
	(void)pr_info("%s ctrl lut_comp %d is %d\n", __func__, lut_index,
		 lut_readl(ctrl_reg));

	// lut table setting
	for (i = 0U; i < LUT_TABLE_SIZE; i++) {
		#if defined(LUT_TABLE_IND_R)
		ind_v = i >> 8U;
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		lut_writel(ind_v, LUT_TABLE_IND_R);
		#endif
		reg_off = (0xFFU & i);
		/* avoid CERT-C Integers Rule INT30-C */
		if (reg_off < (UINT_MAX / 4U)) {
			/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
			lut_writel(table[i], table_reg + (reg_off * 0x4U));
		}
	}

	#if defined(LUT_UPDATE_PEND)
	if (lut_index >= get_vioc_index(VIOC_LUT_COMP0)) {
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		lut_writel((u32)1U << (lut_index - get_vioc_index(VIOC_LUT_COMP0)) |
			   lut_readl(LUT_UPDATE_PEND), LUT_UPDATE_PEND);
		(void)pr_info("%s update_pend lut_comp %d is %d\n", __func__,
			 lut_index, lut_readl(LUT_UPDATE_PEND));
	}
	#endif
}

void tcc_set_lut_csc_coeff(unsigned int lut_csc_11_12,
			   unsigned int lut_csc_13_21,
			   unsigned int lut_csc_22_23,
			   unsigned int lut_csc_31_32, unsigned int lut_csc_32)
{
	#if defined(LUT_COEFFBASE)
	void __iomem *lut_coeff_base_reg = (void __iomem *)LUT_COEFFBASE;

	/* coeff11_12 */
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	lut_writel(lut_csc_11_12, lut_coeff_base_reg);
	/* coeff13_21 */
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	lut_writel(lut_csc_13_21, lut_coeff_base_reg + 0x4U);
	/* coeff22_23 */
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	lut_writel(lut_csc_22_23, lut_coeff_base_reg + 0x8U);
	/* coeff31_32 */
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	lut_writel(lut_csc_31_32, lut_coeff_base_reg + 0xCU);
	/* coeff33 */
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	lut_writel(lut_csc_32, lut_coeff_base_reg + 0x10U);
	#else
	/* avoid MISRA C-2012 Rule 2.7 */
    (void)lut_csc_11_12;
	(void)lut_csc_13_21;
	(void)lut_csc_22_23;
	(void)lut_csc_31_32;
	(void)lut_csc_32;
	#endif
}

void tcc_set_default_lut_csc_coeff(void)
{
	#if defined(LUT_COEFFBASE)
	unsigned int lut_csc_val;
	void __iomem *lut_coeff_base_reg = (void __iomem *)LUT_COEFFBASE;

	lut_csc_val = 1U;
	/* coeff11_12 */
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	lut_writel(lut_csc_val, lut_coeff_base_reg);
	/* coeff13_21 */
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	lut_writel(lut_csc_val, lut_coeff_base_reg + 0x4U);
	/* coeff22_23 */
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	lut_writel(lut_csc_val, lut_coeff_base_reg + 0x8U);
	/* coeff33 */
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	lut_writel(lut_csc_val, lut_coeff_base_reg + 0x10U);

	lut_csc_val = 0U;
	/* coeff31_32 */
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	lut_writel(lut_csc_val, lut_coeff_base_reg + 0xCU);
	#endif
}

void tcc_set_mix_config(int r2y_sel, int bypass)
{
	#if defined(LUT_MIX_CFG)
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	lut_writel(((r2y_sel & 0xF) << 4) | (bypass ? 1 : 0), LUT_MIX_CFG);
	#else
	/* avoid MISRA C-2012 Rule 2.7 */
    (void)r2y_sel;
	(void)bypass;
	#endif
}

/* coverity[HIS_metric_violation : FALSE] */
int tcc_set_lut_plugin(unsigned int lut_n, unsigned int plugComp)
{
	void __iomem *reg;

	int plugin, ret = -1;
	int is_dev = -1;
	unsigned int lut_cfg_val;

	reg = lut_get_address(lut_n, &is_dev);
	if (reg == NULL) {
		(void)pr_err("[ERR][LUT] %s lut number %d is out of range\n",
		       __func__, lut_n);
		ret = -1;
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto FUNC_EXIT;
	}

	if (is_dev == 1) {
		(void)pr_err("[ERR][LUT] %s lut number %d is out of range\n",
		       __func__, lut_n);
		ret = -1;
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto FUNC_EXIT;
	}
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	lut_cfg_val = lut_readl(reg);
	lut_cfg_val &= ~L_CONFIG_SEL_MASK;

	plugin = lut_get_pluginComponent_index(plugComp);
	if (plugin < 0) {
		(void)pr_err("%s plugcomp(0x%x) is out of range\n", __func__,
		       plugComp);
		ret = -1;
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto FUNC_EXIT;
	}
	lut_cfg_val |= ((unsigned int)plugin << L_CONFIG_SEL_SHIFT);
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	lut_writel(lut_cfg_val, reg);
	ret = 0;

FUNC_EXIT:
	return ret;
}

/* coverity[HIS_metric_violation : FALSE] */
int tcc_get_lut_plugin(unsigned int lut_n)
{
	const void __iomem *reg;
	unsigned int value;
	int ret = -1;
	int is_dev = -1;

	reg = lut_get_address(lut_n, &is_dev);
	if (reg == NULL) {
		(void)pr_err("[ERR][LUT] %s lut number %d is out of range\n",
		       __func__, lut_n);
		ret = -1;
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto FUNC_EXIT;
	}

	if (is_dev == 1) {
		(void)pr_err("[ERR][LUT] %s lut number %d is out of range\n",
		       __func__, lut_n);
		ret = -1;
		/* coverity[misra_c_2012_rule_15_1_violation : FALSE] */
		goto FUNC_EXIT;
	}

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	value = lut_readl(reg);
	ret = lut_get_Component_index_to_tvc(value & (L_CONFIG_SEL_MASK));

FUNC_EXIT:
	return ret;
}

void tcc_set_lut_enable(unsigned int lut_n, unsigned int enable)
{
	void __iomem *reg;
	int is_dev = -1;

	reg = lut_get_address(lut_n, &is_dev);
	if (reg == NULL) {
		(void)pr_err("[ERR][LUT] %s lut number %d is out of range\n",
		       __func__, lut_n);
	} else {
		(void)pr_info("%s lut_index(%d) %s\n", __func__, get_vioc_index(lut_n),
			(enable == 1U) ? "enable" : "disable");
		// enable , disable
		if (enable == 1U) {
			/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
			/* coverity[cert_dcl37_c_violation : FALSE] */
			lut_writel(readl(reg) | (L_CFG_EN_MASK), reg);
		} else {
			/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
			/* coverity[cert_dcl37_c_violation : FALSE] */
			lut_writel(readl(reg) & (~(L_CFG_EN_MASK)), reg);
		}
	}
}

int tcc_get_lut_enable(unsigned int lut_n)
{
	const void __iomem *reg;
	int is_dev = -1;
	int ret = -1;

	reg = lut_get_address(lut_n, &is_dev);
	if (reg == NULL) {
		(void)pr_err("[ERR][LUT] %s lut number %d is out of range\n",
		       __func__, lut_n);
		ret = -1;
	} else {
		// enable , disable
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		if ((lut_readl(reg) & ((u32)L_CFG_EN_MASK)) != 0U) {
			/* Prevent KCS warning */
			ret = 1;
		} else {
			/* Prevent KCS warning */
			ret = 0;
		}
	}

	return ret;
}

int tcc_get_lut_update_pend(unsigned int lut_n)
{
	int pend = 0;

	#if defined(LUT_UPDATE_PEND)
	unsigned int lut_index = get_vioc_index(lut_n);

	if (lut_index >= get_vioc_index(VIOC_LUT_COMP0)
	    && lut_index < VIOC_LUT_COMP_MAX) {
	    /* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		pend = lut_readl(LUT_UPDATE_PEND);
	}
	pend = (pend >> (lut_index - get_vioc_index(VIOC_LUT_COMP0)) & 1);
	#else
	/* avoid MISRA C-2012 Rule 2.7 */
    (void)lut_n;
	#endif
	return pend;
}

void __iomem *VIOC_LUT_GetAddress(void)
{
	if (pLUT_reg == NULL) {
		/* Prevent KCS warning */
		(void)pr_err("[ERR][LUT] %s ADDRESS NULL\n", __func__);
	}

	return pLUT_reg;
}

static int __init vioc_lut_init(void)
{
	struct device_node *ViocLUT_np;

	ViocLUT_np = of_find_compatible_node(NULL, NULL, "telechips,vioc_lut");
	if (ViocLUT_np == NULL) {
		/* Prevent KCS warning */
		(void)pr_info("[INF][LUT] disabled\n");
	} else {
		pLUT_reg = (void __iomem *)of_iomap(ViocLUT_np,
			(is_VIOC_REMAP != 0U) ? 1 : 0);

		if (pLUT_reg != NULL) {
		/* Prevent KCS warning */
			(void)pr_info("[INF][LUT] LUT\n");
		}
	}

	return 0;
}
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
arch_initcall(vioc_lut_init);
