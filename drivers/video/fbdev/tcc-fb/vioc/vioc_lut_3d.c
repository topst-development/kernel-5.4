// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * linux/arch/arm/mach-tcc899x/vioc_pixel_mapper.c
 * Author:  <linux@telechips.com>
 * Created: Jan 20, 2018
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
#include <linux/kernel.h>
#include <linux/of_address.h>
#include <linux/wait.h>
#include <linux/delay.h>
#include <video/tcc/vioc_global.h>
#include <video/tcc/vioc_lut_3d.h>

#define LUT0_INDEX	125U
#define LUT1_INDEX	225U
#define LUT2_INDEX	325U
#define LUT3_INDEX	405U
#define LUT4_INDEX	505U
#define LUT5_INDEX	585U
#define LUT6_INDEX	665U
#define LUT7_INDEX	729U

static void __iomem *pLUT3D_reg[VIOC_LUT_3D_MAX] = {0};

static unsigned int lut_reg[729];

int vioc_lut_3d_set_select(unsigned int lut_n, unsigned int sel)
{
	unsigned int value;
	void __iomem *reg;

	reg = get_lut_3d_address(lut_n);

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	value = __raw_readl(reg + LUT_3D_CTRL_OFFSET) & ~(LUT_3D_CTRL_SEL_MASK);
	value |= (sel << LUT_3D_CTRL_SEL_SHIFT);

	__raw_writel(value, reg);
	return 0;
}

int vioc_lut_3d_set_table(unsigned int lut_n, const unsigned int *lut3dtable)
{
	unsigned int idx, i, offset;
	void __iomem *reg;

	reg = get_lut_3d_address(lut_n) + LUT_3D_TABLE_OFFSET;

	idx = 0U;
	(void)vioc_lut_3d_set_select(lut_n, 0U);
	for (i = 0U; i < LUT0_INDEX; i++) {
		offset = (0xFFFU & idx);
		/* avoid CERT-C Integers Rule INT30-C */
		if (offset < (UINT_MAX / 4U)) {
			/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
			__raw_writel(lut3dtable[i], reg + (offset * 0x4U));
		}
		idx++;
	}

	idx = 0;
	(void)vioc_lut_3d_set_select(lut_n, 1U);
	for (i = LUT0_INDEX; i < LUT1_INDEX; i++) {
		offset = (0xFFFU & idx);
		/* avoid CERT-C Integers Rule INT30-C */
		if (offset < (UINT_MAX / 4U)) {
			/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
			__raw_writel(lut3dtable[i], reg + (offset * 0x4U));
		}
		idx++;
	}

	idx = 0;
	(void)vioc_lut_3d_set_select(lut_n, 2U);
	for (i = LUT1_INDEX; i < LUT2_INDEX; i++) {
		offset = (0xFFFU & idx);
		/* avoid CERT-C Integers Rule INT30-C */
		if (offset < (UINT_MAX / 4U)) {
			/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
			__raw_writel(lut3dtable[i], reg + (offset * 0x4U));
		}
		idx++;
	}

	idx = 0;
	(void)vioc_lut_3d_set_select(lut_n, 3U);
	for (i = LUT2_INDEX; i < LUT3_INDEX; i++) {
		offset = (0xFFFU & idx);
		/* avoid CERT-C Integers Rule INT30-C */
		if (offset < (UINT_MAX / 4U)) {
			/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
			__raw_writel(lut3dtable[i], reg + (offset * 0x4U));
		}
		idx++;
	}

	idx = 0;
	(void)vioc_lut_3d_set_select(lut_n, 4U);
	for (i = LUT3_INDEX; i < LUT4_INDEX; i++) {
		offset = (0xFFFU & idx);
		/* avoid CERT-C Integers Rule INT30-C */
		if (offset < (UINT_MAX / 4U)) {
			/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
			__raw_writel(lut3dtable[i], reg + (offset * 0x4U));
		}
		idx++;
	}

	idx = 0;
	(void)vioc_lut_3d_set_select(lut_n, 5U);
	for (i = LUT4_INDEX; i < LUT5_INDEX; i++) {
		offset = (0xFFFU & idx);
		/* avoid CERT-C Integers Rule INT30-C */
		if (offset < (UINT_MAX / 4U)) {
			/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
			__raw_writel(lut3dtable[i], reg + (offset * 0x4U));
		}
		idx++;
	}

	idx = 0;
	(void)vioc_lut_3d_set_select(lut_n, 6U);
	for (i = LUT5_INDEX; i < LUT6_INDEX; i++) {
		offset = (0xFFFU & idx);
		/* avoid CERT-C Integers Rule INT30-C */
		if (offset < (UINT_MAX / 4U)) {
			/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
			__raw_writel(lut3dtable[i], reg + (offset * 0x4U));
		}
		idx++;
	}

	idx = 0;
	(void)vioc_lut_3d_set_select(lut_n, 7U);
	for (i = LUT6_INDEX; i < LUT7_INDEX; i++) {
		offset = (0xFFFU & idx);
		/* avoid CERT-C Integers Rule INT30-C */
		if (offset < (UINT_MAX / 4U)) {
			/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
			__raw_writel(lut3dtable[i], reg + (offset * 0x4U));
		}
		idx++;
	}

	(void)vioc_lut_3d_pend(lut_n, 1U);
	(void)vioc_lut_3d_setupdate(lut_n);

	return 0;
}

int vioc_lut_3d_setupdate(unsigned int lut_n)
{
	unsigned int value;
	void __iomem *reg;

	reg = get_lut_3d_address(lut_n);

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	value = __raw_readl(reg + LUT_3D_CTRL_OFFSET)
		| ((u32)1U << LUT_3D_CTRL_UPD_SHIFT);
	__raw_writel(value, reg + LUT_3D_CTRL_OFFSET);

	return 0;
}

/*
 *	vioc_lut_3d_bypass
 *	@lut_n : 3d LUT NUM
 *	@onoff :
 *		0 - Bypass ON	(Disable 3D LUT)
 *		1 - Bypass OFF	(Enable 3D LUT)
 *
 */
int vioc_lut_3d_bypass(unsigned int lut_n, unsigned int onoff)
{
	unsigned int value;
	void __iomem *reg;

	reg = get_lut_3d_address(lut_n);

	if (onoff == 1U) {
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		value = __raw_readl(reg + LUT_3D_CTRL_OFFSET)
			& ~(LUT_3D_CTRL_BYPASS_MASK);
	} else {
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		value = __raw_readl(reg + LUT_3D_CTRL_OFFSET)
			| (LUT_3D_CTRL_BYPASS_MASK);
	}

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	__raw_writel(value, reg + LUT_3D_CTRL_OFFSET);
	return 0;
}

int vioc_lut_3d_pend(unsigned int lut_n, unsigned int onoff)
{
	unsigned int value;
	void __iomem *reg;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	reg = get_lut_3d_address(lut_n) + LUT_3D_PEND_OFFSET;

	if (onoff == 1U) {
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		value = __raw_readl(reg) | (LUT_3D_PEND_PEND_MASK);
	} else {
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		value = __raw_readl(reg) & ~(LUT_3D_PEND_PEND_MASK);
	}

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	__raw_writel(value, reg);
	return 0;
}

void __iomem *get_lut_3d_address(unsigned int lut_n)
{
	void __iomem *ret = NULL;
	if (lut_n < VIOC_LUT_3D_MAX) {
		/* Prevent KCS warning */
		ret = pLUT3D_reg[lut_n];
	} else {
		/* Prevent KCS warning */
		ret = NULL;
	}

	return ret;
}

static int __init vioc_lut_3d_init(void)
{
	unsigned int i = 0;
	struct device_node *ViocLUT3D_np;

	ViocLUT3D_np = of_find_compatible_node(NULL, NULL,
		"telechips,vioc_lut_3d");

	if (ViocLUT3D_np == NULL) {
		/* Prevent KCS warning */
		(void)pr_info("[INF][LUT_3D] disabled\n");
	} else {
		for (i = 0; i <  VIOC_LUT_3D_MAX; i++) {
			pLUT3D_reg[i] = of_iomap(ViocLUT3D_np, (int)i);

			if (pLUT3D_reg[i] != NULL) {
				/* Prevent KCS warning */
				(void)pr_info("[INF][LUT_3D] vioc-lut-3d%d\n",
					i);
			}

			(void)vioc_lut_3d_bypass(i, 0);
			(void)memset(&lut_reg, 0x0, sizeof(lut_reg));
		}
	}
	return 0;
}
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
arch_initcall(vioc_lut_3d_init);
