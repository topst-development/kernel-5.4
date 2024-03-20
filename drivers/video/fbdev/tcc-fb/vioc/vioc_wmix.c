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
#include <linux/kernel.h>
#include <linux/of_address.h>

#include <video/tcc/tcc_types.h>
#include <video/tcc/vioc_config.h>
#include <video/tcc/vioc_wmix.h>
#include <video/tcc/vioc_global.h>
#include <video/tcc/vioc_ddicfg.h> // is_VIOC_REMAP

static struct device_node *ViocWmixer_np;
static void __iomem *pWMIX_reg[VIOC_WMIX_MAX];

static int OVP_table[30][4] = { /* OVP max = 29, Layer max = 3 */
/*LAYER  0, 1, 2, 3*/
		{2, 1, 0, 3}, /*OVP 0*/
		{2, 0, 1, 3}, /*OVP 1*/
		{1, 2, 0, 3}, /*OVP 2*/
		{1, 0, 2, 3}, /*OVP 3*/
		{0, 2, 1, 3}, /*OVP 4*/
		{0, 1, 2, 3}, /*OVP 5*/
		{-1, -1, -1, -1}, /*OVP 6, Non-use*/
		{-1, -1, -1, -1}, /*OVP 7, Non-use*/
		{3, 1, 0, 2}, /*OVP 8*/
		{3, 0, 1, 2}, /*OVP 9*/
		{1, 3, 0, 2}, /*OVP 10*/
		{1, 0, 3, 2}, /*OVP 11*/
		{0, 3, 1, 2}, /*OVP 12*/
		{0, 1, 3, 2}, /*OVP 13*/
		{-1, -1, -1, -1}, /*OVP 14, Non-use*/
		{-1, -1, -1, -1}, /*OVP 15, Non-use*/
		{3, 2, 0, 1}, /*OVP 16*/
		{3, 0, 2, 1}, /*OVP 17*/
		{2, 3, 0, 1}, /*OVP 18*/
		{2, 0, 3, 1}, /*OVP 19*/
		{0, 3, 2, 1}, /*OVP 20*/
		{0, 2, 3, 1}, /*OVP 21*/
		{-1, -1, -1, -1}, /*OVP 22, Non-use*/
		{-1, -1, -1, -1}, /*OVP 23, Non-use*/
		{3, 2, 1, 0}, /*OVP 24*/
		{3, 1, 2, 0}, /*OVP 25*/
		{2, 3, 1, 0}, /*OVP 26*/
		{2, 1, 3, 0}, /*OVP 27*/
		{1, 3, 2, 0}, /*OVP 28*/
		{1, 2, 3, 0}, /*OVP 29*/
};
#if defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC803X)
static int WMIXER_TYPE[VIOC_WMIX_MAX] = {
	VIOC_WMIX_TYPE_4TO2, /* WMIXER 0 */
	VIOC_WMIX_TYPE_4TO2, /* WMIXER 1 */
	VIOC_WMIX_TYPE_4TO2, /* WMIXER 2 */
	VIOC_WMIX_TYPE_2TO2, /* WMIXER 3 */
	VIOC_WMIX_TYPE_2TO2, /* WMIXER 4 */
	VIOC_WMIX_TYPE_2TO2, /* WMIXER 5 */
	VIOC_WMIX_TYPE_2TO2, /* WMIXER 6 */
};
#endif

#if defined(CONFIG_ARCH_TCC897X)
static int WMIXER_TYPE[VIOC_WMIX_MAX] = {
	VIOC_WMIX_TYPE_4TO2, /* WMIXER 0 */
	VIOC_WMIX_TYPE_4TO2, /* WMIXER 1 */
	-1, /* WMIXER 2, NON-use */
	VIOC_WMIX_TYPE_2TO2, /* WMIXER 3 */
	VIOC_WMIX_TYPE_2TO2, /* WMIXER 4 */
	VIOC_WMIX_TYPE_2TO2, /* WMIXER 5 */
	-1, /* WMIXER 6, NON-use */
};
#endif
static int OVP_BIT0_table[2][2] = { /* OVP max = 1, Layer max = 1 */
/*LAYER  0, 1*/
		{1, 0}, /*OVP[0] 0*/
		{0, 1}, /*OVP[0] 1*/
};

void VIOC_WMIX_SetOverlayPriority(
	void __iomem *reg, unsigned int nOverlayPriority)
{
	u32 val;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	val = (__raw_readl(reg + MCTRL) & ~(MCTRL_OVP_MASK));
	val |= (nOverlayPriority << MCTRL_OVP_SHIFT);
	__raw_writel(val, reg + MCTRL);

}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_WMIX_SetOverlayPriority);

void VIOC_WMIX_GetOverlayPriority(
	const void __iomem *reg, unsigned int *nOverlayPriority)
{
	//	*nOverlayPriority = pWMIX->uCTRL.nREG & 0x1F;
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	*nOverlayPriority =
		(((__raw_readl(reg + MCTRL)) & MCTRL_OVP_MASK)
		 >> MCTRL_OVP_SHIFT);
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_WMIX_GetOverlayPriority);

int VIOC_WMIX_GetLayer(
	unsigned int vioc_id, unsigned int image_num)
{
	unsigned int OverlayPriority;
	int layer;
	int mixer_type;
	const void __iomem *reg = VIOC_WMIX_GetAddress(vioc_id);
	int ret = -1;

	if (image_num > 3U) {
		(void)pr_err("[ERR][WMIX] %s image_num(%d) is wrong\n", __func__, image_num);
		ret = -1;
	} else {
		mixer_type = VIOC_WMIX_GetMixerType(vioc_id);
		VIOC_WMIX_GetOverlayPriority(reg, &OverlayPriority);

		if (mixer_type == VIOC_WMIX_TYPE_4TO2) {
			for (layer = 0; layer < 4; layer++) {
				if ((int)image_num == OVP_table[OverlayPriority][layer]) {
					/* Prevent KCS Warning */
					ret = layer;
				}
			}
		} else if (mixer_type == VIOC_WMIX_TYPE_2TO2) {
			OverlayPriority = OverlayPriority & 0x1U;
			if (image_num > 1U) {
				/* Prevent KCS Warning */
				ret = -1;
			}
			for (layer = 0; layer < 2; layer++) {
				if ((int)image_num ==
					OVP_BIT0_table[OverlayPriority][layer]) {
					/* Prevent KCS Warning */
					ret = layer;
				}
			}
		} else {
			ret = -1;
		}
	}

	if (ret < 0) {
		(void)pr_err("[ERR][WMIX] %s vioc_id:%d image_num:%d\n",
		__func__, vioc_id, image_num);
	}
	return ret;
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_WMIX_GetLayer);

int VIOC_WMIX_GetMixerType(unsigned int vioc_id)
{
	unsigned int Num = get_vioc_index(vioc_id);
	int ret = -1;

	if ((Num >= VIOC_WMIX_MAX) || (WMIXER_TYPE[Num] == -1)) {
		(void)pr_err("[ERR][RDMA] %s Num:%d max num:%d\n", __func__, Num,
		   	VIOC_WMIX_MAX);
		ret = -1;
	} else {
		ret = WMIXER_TYPE[Num];
	}

	return ret;
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_WMIX_GetMixerType);

void VIOC_WMIX_SetUpdate(void __iomem *reg)
{
	u32 val;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	val = (__raw_readl(reg + MCTRL) & ~(MCTRL_UPD_MASK));
	val |= ((u32)0x1U << MCTRL_UPD_SHIFT);
	__raw_writel(val, reg + MCTRL);
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_WMIX_SetUpdate);

void VIOC_WMIX_SetSize(
	void __iomem *reg, unsigned int nWidth, unsigned int nHeight)
{
	u32 val;

	val = (((nHeight & 0x1FFFU) << MSIZE_HEIGHT_SHIFT)
	       | ((nWidth & 0x1FFFU) << MSIZE_WIDTH_SHIFT));
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	__raw_writel(val, reg + MSIZE);
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_WMIX_SetSize);

void VIOC_WMIX_GetSize(
	const void __iomem *reg, unsigned int *nWidth, unsigned int *nHeight)
{
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	*nWidth =
		((__raw_readl(reg + MSIZE) & MSIZE_WIDTH_MASK)
		 >> MSIZE_WIDTH_SHIFT);
	*nHeight =
		((__raw_readl(reg + MSIZE) & MSIZE_HEIGHT_MASK)
		 >> MSIZE_HEIGHT_SHIFT);

}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_WMIX_GetSize);

void VIOC_WMIX_SetBGColor(
	void __iomem *reg, unsigned int nBG0, unsigned int nBG1,
	unsigned int nBG2, unsigned int nBG3)
{
	u32 val;

	val = (((nBG1 & 0xFFU) << MBG_BG1_SHIFT)
	       | ((nBG0 & 0xFFU) << MBG_BG0_SHIFT)
	       | ((nBG3 & 0xFFU) << MBG_BG3_SHIFT)
	       | ((nBG2 & 0xFFU) << MBG_BG2_SHIFT));
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	__raw_writel(val, reg + MBG);
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_WMIX_SetBGColor);

void VIOC_WMIX_SetPosition(
	void __iomem *reg, unsigned int nChannel, unsigned int nX,
	unsigned int nY)
{
	u32 val;

	if (nChannel > 3U) {
		(void)pr_err("[ERR][WMIX] %s nChannel(%d) is wrong\n", __func__, nChannel);
	} else {
		val = (((nY & 0x1FFFU) << MPOS_YPOS_SHIFT)
			| ((nX & 0x1FFFU) << MPOS_XPOS_SHIFT));
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		__raw_writel(val, reg + (MPOS0 + (0x4U * nChannel)));
	}
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_WMIX_SetPosition);

void VIOC_WMIX_GetPosition(
	const void __iomem *reg, unsigned int nChannel, unsigned int *nX,
	unsigned int *nY)
{
	if (nChannel > 3U) {
		(void)pr_err("[ERR][WMIX] %s nChannel(%d) is wrong\n", __func__, nChannel);
	} else {
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		*nX = ((__raw_readl(reg + (MPOS0 + (0x4U * nChannel))) & MPOS_XPOS_MASK)
			>> MPOS_XPOS_SHIFT);
		*nY = ((__raw_readl(reg + (MPOS0 + (0x4U * nChannel))) & MPOS_YPOS_MASK)
			>> MPOS_YPOS_SHIFT);
	}
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_WMIX_GetPosition);

void VIOC_WMIX_SetChromaKey(
	void __iomem *reg, unsigned int nLayer, unsigned int nKeyEn,
	unsigned int nKeyR, unsigned int nKeyG, unsigned int nKeyB,
	unsigned int nKeyMaskR, unsigned int nKeyMaskG, unsigned int nKeyMaskB)
{
	u32 val;

	if (nLayer > 2U) {
		(void)pr_err("[ERR][WMIX] %s nLayer(%d) is wrong\n", __func__, nLayer);
	} else {
		val = (((nKeyEn & 0x1U) << MKEY0_KEN_SHIFT)
			| ((nKeyR & 0xFFU) << MKEY0_KRYR_SHIFT)
			| ((nKeyG & 0xFFU) << MKEY0_KEYG_SHIFT)
			| ((nKeyB & 0xFFU) << MKEY0_KEYB_SHIFT));
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		__raw_writel(val, reg + (MKEY00 + (0x08U * nLayer)));

		val = (((nKeyMaskR & 0xFFU) << MKEY1_MKEYR_SHIFT)
			| ((nKeyMaskG & 0xFFU) << MKEY1_MKEYG_SHIFT)
			| ((nKeyMaskB & 0xFFU) << MKEY1_MKEYB_SHIFT));
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		__raw_writel(val, reg + (MKEY01 + (0x08U * nLayer)));
	}
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_WMIX_SetChromaKey);

void VIOC_WMIX_GetChromaKey(
	const void __iomem *reg, unsigned int nLayer, unsigned int *nKeyEn,
	unsigned int *nKeyR, unsigned int *nKeyG, unsigned int *nKeyB,
	unsigned int *nKeyMaskR, unsigned int *nKeyMaskG,
	unsigned int *nKeyMaskB)
{
	if (nLayer > 2U) {
		(void)pr_err("[ERR][WMIX] %s nLayer(%d) is wrong\n", __func__, nLayer);
	} else {
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		*nKeyEn =
			((__raw_readl(reg + (MKEY00 + (0x08U * nLayer)))
			& MKEY0_KEN_MASK)
			>> MKEY0_KEN_SHIFT);
		*nKeyR =
			((__raw_readl(reg + (MKEY00 + (0x08U * nLayer)))
			& MKEY0_KRYR_MASK)
			>> MKEY0_KRYR_SHIFT);
		*nKeyG =
			((__raw_readl(reg + (MKEY00 + (0x08U * nLayer)))
			& MKEY0_KEYG_MASK)
			>> MKEY0_KEYG_SHIFT);
		*nKeyB =
			((__raw_readl(reg + (MKEY00 + (0x08U * nLayer)))
			& MKEY0_KEYB_MASK)
			>> MKEY0_KEYB_SHIFT);

		*nKeyMaskR =
			((__raw_readl(reg + (MKEY01 + (0x08U * nLayer)))
			& MKEY1_MKEYR_MASK)
			>> MKEY1_MKEYR_SHIFT);
		*nKeyMaskG =
			((__raw_readl(reg + (MKEY01 + (0x08U * nLayer)))
			& MKEY1_MKEYG_MASK)
			>> MKEY1_MKEYG_SHIFT);
		*nKeyMaskB =
			((__raw_readl(reg + (MKEY01 + (0x08U * nLayer)))
			& MKEY1_MKEYB_MASK)
			>> MKEY1_MKEYB_SHIFT);
	}
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_WMIX_GetChromaKey);

void VIOC_WMIX_ALPHA_SetAlphaValueControl(
	void __iomem *reg, unsigned int layer, unsigned int region,
	unsigned int acon0, unsigned int acon1)
{
	u32 val;

	if (layer > 2U) {
		(void)pr_err("[ERR][WMIX] %s layer(%d) is wrong\n", __func__, layer);
	} else {
		switch (region) {
		case 0: /*Region A*/
			/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
			val = (__raw_readl(reg + (MACON0 + (0x20U * layer)))
				& ~(MACON_ACON1_00_MASK | MACON_ACON0_00_MASK));
			val |= (((acon1 & 0x7U) << MACON_ACON1_00_SHIFT)
				| ((acon0 & 0x7U) << MACON_ACON0_00_SHIFT));
			__raw_writel(val, reg + (MACON0 + (0x20U * layer)));
			break;
		case 1: /*Region B*/
			/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
			val = (__raw_readl(reg + (MACON0 + (0x20U * layer)))
				& ~(MACON_ACON1_10_MASK | MACON_ACON0_10_MASK));
			val |= (((acon1 & 0x7U) << MACON_ACON1_10_SHIFT)
				| ((acon0 & 0x7U) << MACON_ACON0_10_SHIFT));
			__raw_writel(val, reg + (MACON0 + (0x20U * layer)));
			break;
		case 2: /*Region C*/
			/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
			val = (__raw_readl(reg + (MACON0 + (0x20U * layer)))
				& ~(MACON_ACON1_11_MASK | MACON_ACON0_11_MASK));
			val |= (((acon1 & 0x7U) << MACON_ACON1_11_SHIFT)
				| ((acon0 & 0x7U) << MACON_ACON0_11_SHIFT));
			__raw_writel(val, reg + (MACON0 + (0x20U * layer)));
			break;
		case 3: /*Region D*/
			/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
			val = (__raw_readl(reg + (MACON0 + (0x20U * layer)))
				& ~(MACON_ACON1_01_MASK | MACON_ACON0_01_MASK));
			val |= (((acon1 & 0x7U) << MACON_ACON1_01_SHIFT)
				| ((acon0 & 0x7U) << MACON_ACON0_01_SHIFT));
			__raw_writel(val, reg + (MACON0 + (0x20U * layer)));
			break;
		default:
			(void)pr_err("[ERR][WMIX] %s region(%d) is wrong\n", __func__, region);
			break;
		}
	}
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_WMIX_ALPHA_SetAlphaValueControl);

void VIOC_WMIX_ALPHA_SetColorControl(
	void __iomem *reg, unsigned int layer, unsigned int region,
	unsigned int ccon0, unsigned int ccon1)
{
	u32 val;

	if (layer > 2U) {
		(void)pr_err("[ERR][WMIX] %s layer(%d) is wrong\n", __func__, layer);
	} else {
		switch (region) {
		case 0: /*Region A*/
			/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
			val = (__raw_readl(reg + (MCCON0 + (0x20U * layer)))
				& ~(MCCON_CCON1_00_MASK | MCCON_CCON0_00_MASK));
			val |= (((ccon1 & 0xFU) << MCCON_CCON1_00_SHIFT)
				| ((ccon0 & 0xFU) << MCCON_CCON0_00_SHIFT));
			__raw_writel(val, reg + (MCCON0 + (0x20U * layer)));
			break;
		case 1: /*Region B*/
			/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
			val = (__raw_readl(reg + (MCCON0 + (0x20U * layer)))
				& ~(MCCON_CCON1_10_MASK | MCCON_CCON0_10_MASK));
			val |= (((ccon1 & 0xFU) << MCCON_CCON1_10_SHIFT)
				| ((ccon0 & 0xFU) << MCCON_CCON0_10_SHIFT));
			__raw_writel(val, reg + (MCCON0 + (0x20U * layer)));
			break;
		case 2: /*Region C*/
			/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
			val = (__raw_readl(reg + (MCCON0 + (0x20U * layer)))
				& ~(MCCON_CCON1_11_MASK | MCCON_CCON0_11_MASK));
			val |= (((ccon1 & 0xFU) << MCCON_CCON1_11_SHIFT)
				| ((ccon0 & 0xFU) << MCCON_CCON0_11_SHIFT));
			__raw_writel(val, reg + (MCCON0 + (0x20U * layer)));
			break;
		case 3: /*Region D*/
			/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
			val = (__raw_readl(reg + (MCCON0 + (0x20U * layer)))
				& ~(MCCON_CCON1_01_MASK | MCCON_CCON0_01_MASK));
			val |= (((ccon1 & 0xFU) << MCCON_CCON1_01_SHIFT)
				| ((ccon0 & 0xFU) << MCCON_CCON0_01_SHIFT));
			__raw_writel(val, reg + (MCCON0 + (0x20U * layer)));
			break;
		default:
			(void)pr_err("[ERR][WMIX] %s region(%d) is wrong\n", __func__, region);
			break;
		}
	}
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_WMIX_ALPHA_SetColorControl);

void VIOC_WMIX_ALPHA_SetROPMode(
	void __iomem *reg, unsigned int layer, unsigned int mode)
{
	u32 val;

	if (layer > 2U) {
		(void)pr_err("[ERR][WMIX] %s layer(%d) is wrong\n", __func__, layer);
	} else {
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		val = (__raw_readl(reg + (MROPC0 + (0x20U * layer)))
			& ~(MROPC_ROPMODE_MASK));
		val |= ((mode & 0x1FU) << MROPC_ROPMODE_SHIFT);
		__raw_writel(val, reg + (MROPC0 + (0x20U * layer)));
	}
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_WMIX_ALPHA_SetROPMode);

void VIOC_WMIX_ALPHA_SetAlphaSelection(
	void __iomem *reg, unsigned int layer, unsigned int asel)
{
	u32 val;

	if (layer > 2U) {
		(void)pr_err("[ERR][WMIX] %s layer(%d) is wrong\n", __func__, layer);
	} else {
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		val = (__raw_readl(reg + (MROPC0 + (0x20U * layer)))
			& ~(MROPC_ASEL_MASK));
		val |= ((asel & 0x3U) << MROPC_ASEL_SHIFT);
		__raw_writel(val, reg + (MROPC0 + (0x20U * layer)));
	}
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_WMIX_ALPHA_SetAlphaSelection);

void VIOC_WMIX_ALPHA_SetAlphaValue(
	void __iomem *reg, unsigned int layer, unsigned int alpha0,
	unsigned int alpha1)
{
	u32 val;

	if (layer > 2U) {
		(void)pr_err("[ERR][WMIX] %s layer(%d) is wrong\n", __func__, layer);
	} else {
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		val = (__raw_readl(reg + (MROPC0 + (0x20U * layer)))
			& ~(MROPC_ALPHA1_MASK | MROPC_ALPHA0_MASK));
		val |= (((alpha1 & 0xFFU) << MROPC_ALPHA1_SHIFT)
			| ((alpha0 & 0xFFU) << MROPC_ALPHA0_SHIFT));
		__raw_writel(val, reg + (MROPC0 + (0x20U * layer)));
	}
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_WMIX_ALPHA_SetAlphaValue);

/* Not Used (will be deleted) */
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
void VIOC_WMIX_ALPHA_SetROPPattern(
	void __iomem *reg, unsigned int layer, unsigned int patR,
	unsigned int patG, unsigned int patB)
{
	u32 val;

	if (layer > 2U) {
		(void)pr_err("[ERR][WMIX] %s layer(%d) is wrong\n", __func__, layer);
	} else {
		val = (((patB & 0xFFU) << MPAT_BLUE_SHIFT)
			| ((patG & 0xFFU) << MPAT_GREEN_SHIFT)
			| ((patR & 0xFFU) << MPAT_RED_SHIFT));
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		__raw_writel(val, reg + (MPAT0 + (0x20U * layer)));
	}
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_WMIX_ALPHA_SetROPPattern);
#endif

void VIOC_WMIX_SetInterruptMask(void __iomem *reg, unsigned int nMask)
{
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	__raw_writel(nMask, reg + MIRQMSK);
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_WMIX_SetInterruptMask);

unsigned int VIOC_WMIX_GetStatus(const void __iomem *reg)
{
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	return __raw_readl(reg + MSTS);
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_WMIX_GetStatus);

void VIOC_WMIX_DUMP(const void __iomem *reg, unsigned int vioc_id)
{
	unsigned int cnt = 0;
	const void __iomem *pReg = reg;
	unsigned int Num = get_vioc_index(vioc_id);

	if (Num >= VIOC_WMIX_MAX) {
		(void)pr_err("[ERR][WMIX] %s Num:%d max:%d\n",
		__func__, Num, VIOC_WMIX_MAX);
	} else {
		if (pReg == NULL) {
			/* Prevent KCS warning */
			pReg = VIOC_WMIX_GetAddress(vioc_id);
		}
		if (pReg != NULL) {
			(void)pr_info("[DBG][WMIX] WMIX-%d ::\n", Num);
			while (cnt < 0x70U) {

				/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
				(void)pr_info(
					"WMIX-%d + 0x%x: 0x%08x 0x%08x 0x%08x 0x%08x\n", Num, cnt,
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
EXPORT_SYMBOL(VIOC_WMIX_DUMP);

void __iomem *VIOC_WMIX_GetAddress(unsigned int vioc_id)
{
	unsigned int Num = get_vioc_index(vioc_id);
	void __iomem *ret = NULL;

	if ((Num >= VIOC_WMIX_MAX) || (pWMIX_reg[Num] == NULL)) {
		(void)pr_err("[ERR][WMIX] %s Num:%d max:%d\n",
		__func__, Num, VIOC_WMIX_MAX);
		ret = NULL;
	} else {
		ret = pWMIX_reg[Num];
	}

	return ret;
}
/* coverity[misra_c_2012_rule_5_2_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_3_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_5_violation : FALSE] */
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
EXPORT_SYMBOL(VIOC_WMIX_GetAddress);

static int __init vioc_wmixer_init(void)
{
	unsigned int i;

	ViocWmixer_np =
		of_find_compatible_node(NULL, NULL, "telechips,vioc_wmix");
	if (ViocWmixer_np == NULL) {
		/* Prevent KCS warning */
		(void)pr_info("[INF][WMIX] disabled\n");
	} else {
		for (i = 0; i < VIOC_WMIX_MAX; i++) {
			pWMIX_reg[i] = (void __iomem *)of_iomap(
				ViocWmixer_np,
				(is_VIOC_REMAP != 0U) ? ((int)i + (int)VIOC_WMIX_MAX) : (int)i);

			if (pWMIX_reg[i] != NULL) {
				/* Prevent KCS warning */
				(void)pr_info("[INF][WMIX] vioc-wmix%d\n", i);
			}
		}
	}
	return 0;
}
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
arch_initcall(vioc_wmixer_init);
