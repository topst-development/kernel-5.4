// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * linux/arch/arm/mach-tcc893x/vioc_vin.c
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
#include <linux/kernel.h>
#include <linux/of_address.h>

#include <video/tcc/vioc_global.h>
#include <video/tcc/vioc_ddicfg.h> // is_VIOC_REMAP
#include <video/tcc/vioc_intr.h>
#include <video/tcc/vioc_vin.h>

#define VIOC_VIN_IREQ_UPD_MASK 0x00000001U
#define VIOC_VIN_IREQ_EOF_MASK 0x00000002U
#define VIOC_VIN_IREQ_VS_MASK 0x00000004U
#define VIOC_VIN_IREQ_INVS_MASK 0x00000008U

static void __iomem *pVIN_reg[VIOC_VIN_MAX] = {0};

/* VIN polarity Setting
 *	- VIN_CTRL
 *		. [13] = gen_field_en
 *		. [12] = de_active_low
 *		. [11] = field_bfield_low
 *		. [10] = vs_active_low
 *		. [ 9] = hs_active_low
 *		. [ 8] = pxclk_pol
 */
void VIOC_VIN_SetSyncPolarity(
	void __iomem *reg,
	unsigned int hs_active_low,
	unsigned int vs_active_low,
	unsigned int field_bfield_low,
	unsigned int de_active_low,
	unsigned int gen_field_en, 
	unsigned int pxclk_pol)
{
	u32 val;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	val = (__raw_readl(reg + VIN_CTRL)
	       & ~(VIN_CTRL_GFEN_MASK | VIN_CTRL_DEAL_MASK | VIN_CTRL_FOL_MASK
		   | VIN_CTRL_VAL_MASK | VIN_CTRL_HAL_MASK
		   | VIN_CTRL_PXP_MASK));
	val |= (((gen_field_en & 0x1U) << VIN_CTRL_GFEN_SHIFT)
		| ((de_active_low & 0x1U) << VIN_CTRL_DEAL_SHIFT)
		| ((field_bfield_low & 0x1U) << VIN_CTRL_FOL_SHIFT)
		| ((vs_active_low & 0x1U) << VIN_CTRL_VAL_SHIFT)
		| ((hs_active_low & 0x1U) << VIN_CTRL_HAL_SHIFT)
		| ((pxclk_pol & 0x1U) << VIN_CTRL_PXP_SHIFT));
	__raw_writel(val, reg + VIN_CTRL);
}

/* VIN Configuration 1
 *	- VIN_CTRL
 *		. [22:20] = data_order
 *		. [19:16] = fmt
 *		. [ 6] = vs_mask
 *		. [ 4] = hsde_connect_en
 *		. [ 1] = conv_en
 */
void VIOC_VIN_SetCtrl(
	void __iomem *reg, unsigned int conv_en,
	unsigned int hsde_connect_en, unsigned int vs_mask, unsigned int fmt,
	unsigned int data_order)
{
	u32 val;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	val = (__raw_readl(reg + VIN_CTRL)
	       & ~(VIN_CTRL_CONV_MASK | VIN_CTRL_HDCE_MASK | VIN_CTRL_VM_MASK
		   | VIN_CTRL_FMT_MASK | VIN_CTRL_DO_MASK));
	val |= (((conv_en & 0x1U) << VIN_CTRL_CONV_SHIFT)
		| ((hsde_connect_en & 0x1U) << VIN_CTRL_HDCE_SHIFT)
		| ((vs_mask & 0x1U) << VIN_CTRL_VM_SHIFT)
		| ((fmt & 0xFU) << VIN_CTRL_FMT_SHIFT)
		| ((data_order & 0x7U) << VIN_CTRL_DO_SHIFT));
	__raw_writel(val, reg + VIN_CTRL);
}

/* Interlace mode setting
 *	- VIN_CTRL
 *		. [ 3] = intpl_en
 *		. [ 2] = intl_en
 */
void VIOC_VIN_SetInterlaceMode(
	void __iomem *reg, unsigned int intl_en, unsigned int intpl_en)
{
	u32 val;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	val = (__raw_readl(reg + VIN_CTRL)
	       & ~(VIN_CTRL_INTEN_MASK | VIN_CTRL_INTPLEN_MASK));
	val |= (((intl_en & 0x1U) << VIN_CTRL_INTEN_SHIFT)
		| ((intpl_en & 0x1U) << VIN_CTRL_INTPLEN_SHIFT));
	__raw_writel(val, reg);
}

/* VIN Capture mode Enable
 *	- VIN_CTRL
 *		. [31] = cap_en
 */
void VIOC_VIN_SetCaptureModeEnable(
	void __iomem *reg, unsigned int cap_en)
{
	u32 val;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	val = (__raw_readl(reg + VIN_CTRL) & ~(VIN_CTRL_CP_MASK));
	val |= ((cap_en & 0x1U) << VIN_CTRL_CP_SHIFT);
	__raw_writel(val, reg + VIN_CTRL);
}

/* VIN Skip Frame Number
 *	- VIN_CTRL
 *		. [24] = skip
 */
void VIOC_VIN_SetFrameSkipNumber(void __iomem *reg, unsigned int skip)
{
	u32 val;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	val = (__raw_readl(reg + VIN_CTRL) & ~(VIN_CTRL_SKIP_MASK));
	val |= ((skip & 0x1U) << VIN_CTRL_SKIP_SHIFT);
	__raw_writel(val, reg + VIN_CTRL);
}

/* VIN Enable/Disable
 *	- VIN_CTRL
 *		. [ 0] = vin_en
 */
void VIOC_VIN_SetEnable(void __iomem *reg, unsigned int vin_en)
{
	u32 val;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	val = (__raw_readl(reg + VIN_CTRL) & ~(VIN_CTRL_EN_MASK));
	val |= ((vin_en & 0x1U) << VIN_CTRL_EN_SHIFT);
	__raw_writel(val, reg + VIN_CTRL);
}

/* VIN Is Enabled/Disabled
 *	- VIN_CTRL
 *		. return [ 0]
 */
unsigned int VIOC_VIN_IsEnable(const void __iomem *reg)
{
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	return ((__raw_readl(reg + VIN_CTRL) & (VIN_CTRL_EN_MASK))
		>> VIN_CTRL_EN_SHIFT);
}

/* Image size setting
 *	- VIN_SIZE
 *		. [31:16] = height
 *		. [15: 0] = width
 */
void VIOC_VIN_SetImageSize(
	void __iomem *reg, unsigned int width, unsigned int height)
{
	u32 val;

	val = (((height & 0xFFFFU) << VIN_SIZE_HEIGHT_SHIFT)
	       | ((width & 0xFFFFU) << VIN_SIZE_WIDTH_SHIFT));
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	__raw_writel(val, reg + VIN_SIZE);
}

/* Image offset setting
 *	- VIN_OFFS
 *		. [31:16] = offs_height
 *		. [15: 0] = offs_width
 *	- VIN_OFFS_INTL
 *		. [31:16] = offs_height_intl
 */
void VIOC_VIN_SetImageOffset(
	void __iomem *reg, unsigned int offs_width,
	unsigned int offs_height, unsigned int offs_height_intl)
{
	u32 val;

	val = (((offs_height & 0xFFFFU) << VIN_OFFS_OFS_HEIGHT_SHIFT)
	       | ((offs_width & 0xFFFFU) << VIN_OFFS_OFS_WIDTH_SHIFT));
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	__raw_writel(val, reg + VIN_OFFS);

	val = ((offs_height_intl & 0xFFFFU) << VIN_OFFS_INTL_OFS_HEIGHT_SHIFT);
	__raw_writel(val, reg + VIN_OFFS_INTL);
}

/* Image crop size setting
 *	- VIN_CROP_SIZE
 *		. [31:16] = height
 *		. [15: 0] = width
 */
void VIOC_VIN_SetImageCropSize(
	void __iomem *reg, unsigned int width, unsigned int height)
{
	u32 val;

	val = (((height & 0xFFFFU) << VIN_CROP_SIZE_HEIGHT_SHIFT)
	       | ((width & 0xFFFFU) << VIN_CROP_SIZE_WIDTH_SHIFT));
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	__raw_writel(val, reg + VIN_CROP_SIZE);
}

/* Image crop offset(starting porint) setting
 *	- VIN_CROP_OFFS
 *		. [31:16] = offs_height
 *		. [15: 0] = offs_width
 */
void VIOC_VIN_SetImageCropOffset(
	void __iomem *reg, unsigned int offs_width,
	unsigned int offs_height)
{
	u32 val;

	val = (((offs_height & 0xFFFFU) << VIN_CROP_OFFS_OFS_HEIGHT_SHIFT)
	       | ((offs_width & 0xFFFFU) << VIN_CROP_OFFS_OFS_WIDTH_SHIFT));
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	__raw_writel(val, reg + VIN_CROP_OFFS);
}

/* Y2R conversion mode setting
 *	- VIN_MISC
 *		. [ 7: 5] = y2r_mode
 */
void VIOC_VIN_SetY2RMode(void __iomem *reg, unsigned int y2r_mode)
{
	u32 val;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	val = (__raw_readl(reg + VIN_MISC) & ~(VIN_MISC_Y2RM_MASK));
	val |= ((y2r_mode & 0x7U) << VIN_MISC_Y2RM_SHIFT);
	__raw_writel(val, reg + VIN_MISC);
}

/* Y2R conversion Enable/Disable
 *	- VIN_MISC
 *		. [ 4] = y2r_en
 */
void VIOC_VIN_SetY2REnable(void __iomem *reg, unsigned int y2r_en)
{
	u32 val;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	val = (__raw_readl(reg + VIN_MISC) & ~(VIN_MISC_Y2REN_MASK));
	val |= ((y2r_en & 0x1U) << VIN_MISC_Y2REN_SHIFT);
	__raw_writel(val, reg + VIN_MISC);
}

/* initialize LUT, for example, LUT values are set to inverse function.
 *	- VIN_MISC
 *		. [ 3] is controlled in this function automatically
 *	- VIN_LUT_C0
 *		. [31: 0] = (pLUT + 0Byte) ~ +256Byte
 *	- VIN_LUT_C1
 *		. [31: 0] = (pLUT + 256Byte) ~ +256Byte
 *	- VIN_LUT_C2
 *		. [31: 0] = (pLUT + 512Byte) ~ +256Byte
 */
void VIOC_VIN_SetLUT(void __iomem *reg, const unsigned int *pLUT)
{
	const unsigned int *pLUT0 = NULL, *pLUT1 = NULL, *pLUT2 = NULL;
	unsigned int uiCount;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	pLUT0 = pLUT + 0U;
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	pLUT1 = pLUT0 + (256U / 4U);
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	pLUT2 = pLUT1 + (256U / 4U);
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	__raw_writel(
		((__raw_readl(reg + VIN_MISC) & ~(VIN_MISC_LUTIF_MASK))
		 | ((u32)0x1U << VIN_MISC_LUTIF_SHIFT)),
		reg + VIN_MISC); /* Access Look-Up Table Using Slave Port */

	for (uiCount = 0U; uiCount < 256U; uiCount += 4U) {
		/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
		__raw_writel(*pLUT0, reg + VIN_LUT_C + 0x000U + uiCount);
		__raw_writel(*pLUT1, reg + VIN_LUT_C + 0x100U + uiCount);
		__raw_writel(*pLUT2, reg + VIN_LUT_C + 0x200U + uiCount);
		pLUT0++;
		pLUT1++;
		pLUT2++;
	}
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	__raw_writel(
		(__raw_readl(reg + VIN_MISC) & ~(VIN_MISC_LUTIF_MASK)),
		reg + VIN_MISC); /* Access Look-Up Table Using Vin Module */
}

/* LUT Enable/Disable
 *	- VIN_MISC
 *		. [ 2] = lut2_en
 *		. [ 1] = lut1_en
 *		. [ 0] = lut0_en
 */
void VIOC_VIN_SetLUTEnable(
	void __iomem *reg, unsigned int lut0_en, unsigned int lut1_en,
	unsigned int lut2_en)
{
	u32 val;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	val = (__raw_readl(reg + VIN_MISC) & ~(VIN_MISC_LUTEN_MASK));
	val |= (((lut0_en & 0x1U) << VIN_MISC_LUTEN_SHIFT)
		| ((lut1_en & 0x1U) << (VIN_MISC_LUTEN_SHIFT + 1U))
		| ((lut2_en & 0x1U) << (VIN_MISC_LUTEN_SHIFT + 2U)));
	__raw_writel(val, reg + VIN_MISC);
}

/* Not Used (will be deleted) */
void VIOC_VIN_SetDemuxPort(
	void __iomem *reg, unsigned int p0, unsigned int p1,
	unsigned int p2, unsigned int p3)
{
	u32 val;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	val = (__raw_readl(reg + VD_CTRL)
	       & ~(VD_CTRL_SEL3_MASK | VD_CTRL_SEL2_MASK | VD_CTRL_SEL1_MASK
		   | VD_CTRL_SEL0_MASK));
	val |= (((p0 & 0x7U) << VD_CTRL_SEL0_SHIFT)
		| ((p1 & 0x7U) << VD_CTRL_SEL1_SHIFT)
		| ((p2 & 0x7U) << VD_CTRL_SEL2_SHIFT)
		| ((p3 & 0x7U) << VD_CTRL_SEL3_SHIFT));
	__raw_writel(val, reg + VD_CTRL);
}

/* Not Used (will be deleted) */
void VIOC_VIN_SetDemuxClock(void __iomem *reg, unsigned int mode)
{
	u32 val;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	val = (__raw_readl(reg + VD_CTRL) & ~(VD_CTRL_CM_MASK));
	val |= ((mode & 0x7U) << VD_CTRL_CM_SHIFT);
	__raw_writel(val, reg + VD_CTRL);
}

/* Not Used (will be deleted) */
void VIOC_VIN_SetDemuxEnable(void __iomem *reg, unsigned int enable)
{
	u32 val;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	val = (__raw_readl(reg + VD_CTRL) & ~(VD_CTRL_EN_MASK));
	val |= ((enable & 0x1U) << VD_CTRL_EN_SHIFT);
	__raw_writel(val, reg + VD_CTRL);
}

/* VIN Set SE (for mipi input)
 *	- VIN_CTRL
 *		. [14] = se
 */
void VIOC_VIN_SetSEEnable(void __iomem *reg, unsigned int se)
{
	u32 val;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	val = (__raw_readl(reg + VIN_CTRL) & ~(VIN_CTRL_SE_MASK));
	val |= (((se & 0x1U) << VIN_CTRL_SE_SHIFT));
	__raw_writel(val, reg + VIN_CTRL);
}

/* VIN Flush buffer (for mipi input)
 *	- VIN_MISC
 *		. [16] = fvs
 */
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
void VIOC_VIN_SetFlushBufferEnable(void __iomem *reg, unsigned int fvs)
{
	u32 val;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	val = (__raw_readl(reg + VIN_MISC) & ~(VIN_MISC_FVS_MASK));
	val |= ((fvs & 0x1U) << VIN_MISC_FVS_SHIFT);
	__raw_writel(val, reg + VIN_MISC);
}
#endif

#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
/* VIN Interrupt Mask
 *	- VIN_INT
 *		. [19:16] = mask and set
 *	Not Used, Please use the alternative functions in vioc_intr.c
 */
void VIOC_VIN_SetIreqMask(
	void __iomem *reg, unsigned int mask, unsigned int set)
{
	/*
	 * set 1 : IREQ Masked(interrupt enable),
	 * set 0 : IREQ UnMasked(interrput disable)
	 */
	u32 value;

	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	value = (__raw_readl(reg + VIN_INT) & ~(mask));

	if (set == 1U) { /* Interrupt Enable*/
		/* Prevent KCS warning */
		value |= mask;
	} else {
		/* Prevent KCS warining */
		value |= ~mask;
	}

	__raw_writel(value, reg + VIN_INT);
}

/* VIN Get Interrupt Status
 *	- VIN_INT
 *		. return [ 3: 2]
 *	Not Used, Please use the alternative functions in vioc_intr.c
 */
void VIOC_VIN_GetStatus(const void __iomem *reg, unsigned int *status)
{
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	*status = __raw_readl(reg + VIN_INT) & VIOC_VIN_INT_MASK;
}

/* VIN Clear All Monitoring Registers
 *	- VIN_MON_CLR
 *		. [ 0] = 1
 *	* The monitoring registers are used for debugging only.
 */
void VIOC_VIN_ClearAllMonitorCounters(void __iomem *reg)
{
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	__raw_writel(1, reg + VIN_MON_CLR);
}

/* VIN Get HSync Max Count
 *	- VIN_MON_HS
 *		. return [31: 0]
 *	* The monitoring registers are used for debugging only.
 */
unsigned int VIOC_VIN_GetHSyncMax(const void __iomem *reg)
{
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	return (__raw_readl(reg + VIN_MON_HS) & VIN_MON_HS_MAX_MASK)
		>> VIN_MON_HS_MAX_SHIFT;
}

/* VIN Get HSync Current Count
 *	- VIN_MON_HS
 *		. return [31: 0]
 *	* The monitoring registers are used for debugging only.
 */
unsigned int VIOC_VIN_GetHSyncCounter(const void __iomem *reg)
{
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	return (__raw_readl(reg + VIN_MON_HS) & VIN_MON_HS_CNT_MASK)
		>> VIN_MON_HS_CNT_SHIFT;
}

/* VIN Get DataEnable Max Count
 *	- VIN_MON_DE
 *		. return [31:16]
 *	* The monitoring registers are used for debugging only.
 */
unsigned int VIOC_VIN_GetDataEnableMax(const void __iomem *reg)
{
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	return (__raw_readl(reg + VIN_MON_DE) & VIN_MON_DE_MAX_MASK)
		>> VIN_MON_DE_MAX_SHIFT;
}

/* VIN Get DataEnable Current Count
 *	- VIN_MON_DE
 *		. return [31: 0]
 *	* The monitoring registers are used for debugging only.
 */
unsigned int VIOC_VIN_GetDataEnableCounter(const void __iomem *reg)
{
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	return (__raw_readl(reg + VIN_MON_DE) & VIN_MON_DE_CNT_MASK)
		>> VIN_MON_DE_CNT_SHIFT;
}

/* VIN Get LineCounter Max Count
 *	- VIN_MON_LCNT
 *		. return [31:16]
 *	* The monitoring registers are used for debugging only.
 */
unsigned int VIOC_VIN_GetLineCountMax(const void __iomem *reg)
{
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	return (__raw_readl(reg + VIN_MON_LCNT) & VIN_MON_LCNT_MAX_MASK)
		>> VIN_MON_LCNT_MAX_SHIFT;
}

/* VIN Get LineCounter Max Count
 *	- VIN_MON_LCNT
 *		. return [31: 0]
 *	* The monitoring registers are used for debugging only.
 */
unsigned int VIOC_VIN_GetLineCountCounter(const void __iomem *reg)
{
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	return (__raw_readl(reg + VIN_MON_LCNT) & VIN_MON_LCNT_CNT_MASK)
		>> VIN_MON_LCNT_CNT_SHIFT;
}

/* VIN Get VSync Max Count
 *	- VIN_MON_VSMAX
 *		. return [31: 0]
 *	* The monitoring registers are used for debugging only.
 */
unsigned int VIOC_VIN_GetVSyncMax(const void __iomem *reg)
{
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	return __raw_readl(reg + VIN_MON_VSMAX);
}

/* VIN Get VSync Current Count
 *	- VIN_MON_VSCNT
 *		. return [31: 0]
 *	* The monitoring registers are used for debugging only.
 */
unsigned int VIOC_VIN_GetVSyncCounter(const void __iomem *reg)
{
	/* coverity[misra_c_2012_rule_18_4_violation : FALSE] */
	return __raw_readl(reg + VIN_MON_VSCNT);
}
#endif

/* VIN Get Register Address
 *	- return VINn_BASE_ADDR
 */
void __iomem *VIOC_VIN_GetAddress(unsigned int Num)
{
	void __iomem *ret = NULL;

	Num = get_vioc_index(Num);
	if ((Num >= VIOC_VIN_MAX) || (pVIN_reg[Num] == NULL)) {
		(void)pr_err("[ERR][VIN] %s Num:%d , max :%d\n", __func__, Num,
			VIOC_VIN_MAX);
		ret = NULL;
	} else {
		ret = pVIN_reg[Num];
	}

	return ret;
}

/* VIN Init vin structure
 *	- parse the device tree and init vin structure
 */
static int __init vioc_vin_init(void)
{
	unsigned int i;
	struct device_node *ViocVin_np;

	ViocVin_np = of_find_compatible_node(NULL, NULL, "telechips,vioc_vin");
	if (ViocVin_np == NULL) {
		/* Prevent KCS warning */
		(void)pr_info("[INF][VIN] disabled\n");
	} else {
		for (i = 0; i < VIOC_VIN_MAX; i++) {
			pVIN_reg[i] = (void __iomem *)of_iomap(
				ViocVin_np,
				(is_VIOC_REMAP != 0U) ? ((int)i + (int)VIOC_VIN_MAX) : (int)i);

			if (pVIN_reg[i] != NULL) {
				/* Prevent KCS warning */
				(void)pr_info("[INF][VIN] vioc-vin%d\n", i);
			}
		}
	}

	return 0;
}
/* coverity[misra_c_2012_rule_20_7_violation : FALSE] */
/* coverity[misra_c_2012_rule_21_2_violation : FALSE] */
/* coverity[cert_dcl37_c_violation : FALSE] */
arch_initcall(vioc_vin_init);
