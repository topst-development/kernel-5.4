/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips, Inc.
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
#ifndef VIOC_VIN_H
#define	VIOC_VIN_H

#define ORDER_RGB (0U)
#define ORDER_RBG (1U)
#define ORDER_GRB (2U)
#define ORDER_GBR (3U)
#define ORDER_BRG (4U)
#define ORDER_BGR (5U)

#define FMT_YUV422_16BIT   (0U)
#define FMT_YUV422_8BIT    (1U)
#define FMT_YUVK4444_16BIT (2U)
#define FMT_YUVK4224_24BIT (3U)
#define FMT_RGBK4444_16BIT (4U)
#define FMT_RGB444_24BIT   (9U)
#define FMT_SD_PROG        (12U) // NOT USED

#define DE_ACTIVE_HIGH		(0U)
#define DE_ACTIVE_LOW		(1U)
#define VS_ACTIVE_HIGH		(0U)
#define VS_ACTIVE_LOW		(1U)
#define HS_ACTIVE_HIGH		(0U)
#define HS_ACTIVE_LOW		(1U)
#define PCLK_ACTIVE_HIGH	(0U)
#define PCLK_ACTIVE_LOW		(1U)

#define CLK_DOUBLE_EDGE       (0U)
#define CLK_DOUBLE_FREQ       (1U)
#define CLK_DOUBLE_EDGE_FREQ  (2U)
#define CLK_DOUBLE_4TIME_FREQ (3U)

#ifdef ON
#undef ON
#endif

#ifdef OFF
#undef OFF
#endif

#define ON  (1)
#define OFF (0)

/*
 * Register offset
 */
#define	VIN_CTRL			(0x000U)
#define	VIN_MISC			(0x004U)
#define	VIN_SYNC_M0			(0x008U)
#define	VIN_SYNC_M1			(0x00CU)
#define	VIN_SIZE			(0x010U)
#define	VIN_OFFS			(0x014U)
#define	VIN_OFFS_INTL		(0x018U)
#define	VIN_CROP_SIZE		(0x01CU)
#define	VIN_CROP_OFFS		(0x020U)
#define	VIN_MON_CLR			(0x024U)
#define	VIN_MON_HS			(0x028U)
#define	VIN_MON_DE			(0x02CU)
#define	VIN_MON_LCNT		(0x030U)
#define	VIN_MON_VSCNT		(0x034U)
#define	VIN_MON_VSMAX		(0x038U)
#define	VIN_LUT_CTRL		(0x05CU)
#define	VIN_INT				(0x060U)
#define	VIN_LUT_C			(0x400U)

/*
 * VIN Control Register
 */
#define VIN_CTRL_CP_SHIFT		(31U)
#define VIN_CTRL_SKIP_SHIFT		(24U)
#define VIN_CTRL_DO_SHIFT		(20U)
#define VIN_CTRL_FMT_SHIFT		(16U)
#define VIN_CTRL_SE_SHIFT		(14U)
#define VIN_CTRL_GFEN_SHIFT		(13U)
#define VIN_CTRL_DEAL_SHIFT		(12U)
#define VIN_CTRL_FOL_SHIFT		(11U)
#define VIN_CTRL_VAL_SHIFT		(10U)
#define VIN_CTRL_HAL_SHIFT		(9U)
#define VIN_CTRL_PXP_SHIFT		(8U)
#define VIN_CTRL_VM_SHIFT		(6U)
#define VIN_CTRL_FLUSH_SHIFT	(5U)
#define VIN_CTRL_HDCE_SHIFT		(4U)
#define VIN_CTRL_INTPLEN_SHIFT	(3U)
#define VIN_CTRL_INTEN_SHIFT	(2U)
#define VIN_CTRL_CONV_SHIFT		(1U)
#define VIN_CTRL_EN_SHIFT		(0U)

#define VIN_CTRL_CP_MASK		((u32)0x1U << VIN_CTRL_CP_SHIFT)
#define VIN_CTRL_SKIP_MASK		((u32)0xFU << VIN_CTRL_SKIP_SHIFT)
#define VIN_CTRL_DO_MASK		((u32)0x7U << VIN_CTRL_DO_SHIFT)
#define VIN_CTRL_FMT_MASK		((u32)0xFU << VIN_CTRL_FMT_SHIFT)
#define VIN_CTRL_SE_MASK		((u32)0x1U << VIN_CTRL_SE_SHIFT)
#define VIN_CTRL_GFEN_MASK		((u32)0x1U << VIN_CTRL_GFEN_SHIFT)
#define VIN_CTRL_DEAL_MASK		((u32)0x1U << VIN_CTRL_DEAL_SHIFT)
#define VIN_CTRL_FOL_MASK		((u32)0x1U << VIN_CTRL_FOL_SHIFT)
#define VIN_CTRL_VAL_MASK		((u32)0x1U << VIN_CTRL_VAL_SHIFT)
#define VIN_CTRL_HAL_MASK		((u32)0x1U << VIN_CTRL_HAL_SHIFT)
#define VIN_CTRL_PXP_MASK		((u32)0x1U << VIN_CTRL_PXP_SHIFT)
#define VIN_CTRL_VM_MASK		((u32)0x1U << VIN_CTRL_VM_SHIFT)
#define VIN_CTRL_FLUSH_MASK		((u32)0x1U << VIN_CTRL_FLUSH_SHIFT)
#define VIN_CTRL_HDCE_MASK		((u32)0x1U << VIN_CTRL_HDCE_SHIFT)
#define VIN_CTRL_INTPLEN_MASK	((u32)0x1U << VIN_CTRL_INTPLEN_SHIFT)
#define VIN_CTRL_INTEN_MASK		((u32)0x1U << VIN_CTRL_INTEN_SHIFT)
#define VIN_CTRL_CONV_MASK		((u32)0x1U << VIN_CTRL_CONV_SHIFT)
#define VIN_CTRL_EN_MASK		((u32)0x1U << VIN_CTRL_EN_SHIFT)

/*
 * VIN Misc. Register
 */
#define VIN_MISC_VS_DELAY_SHIFT		(20U)
#define VIN_MISC_FVS_SHIFT			(16U)
#define VIN_MISC_ALIGN_SHIFT		(12U)
#define VIN_MISC_R2YM_SHIFT			(9U)
#define VIN_MISC_R2YEN_SHIFT		(8U)
#define VIN_MISC_Y2RM_SHIFT			(5U)
#define VIN_MISC_Y2REN_SHIFT		(4U)
#define VIN_MISC_LUTIF_SHIFT		(3U)
#define VIN_MISC_LUTEN_SHIFT		(0U)

#define VIN_MISC_VS_DELAY_MASK		((u32)0xFU << VIN_MISC_VS_DELAY_SHIFT)
#define VIN_MISC_FVS_MASK			((u32)0x1U << VIN_MISC_FVS_SHIFT)
#define VIN_MISC_ALIGN_MASK			((u32)0x3U << VIN_MISC_ALIGN_SHIFT)
#define VIN_MISC_R2YM_MASK			((u32)0x7U << VIN_MISC_R2YM_SHIFT)
#define VIN_MISC_R2YEN_MASK			((u32)0x1U << VIN_MISC_R2YEN_SHIFT)
#define VIN_MISC_Y2RM_MASK			((u32)0x7U << VIN_MISC_Y2RM_SHIFT)
#define VIN_MISC_Y2REN_MASK			((u32)0x1U << VIN_MISC_Y2REN_SHIFT)
#define VIN_MISC_LUTIF_MASK			((u32)0x1U << VIN_MISC_LUTIF_SHIFT)
#define VIN_MISC_LUTEN_MASK			((u32)0x7U << VIN_MISC_LUTEN_SHIFT)

/*
 * VIN Sync Misc. 0 Register
 */
#define VIN_SYNC_M0_SB_SHIFT		(18U)
#define VIN_SYNC_M0_PSL_SHIFT		(16U)
#define VIN_SYNC_M0_FP_SHIFT		(8U)
#define VIN_SYNC_M0_VB_SHIFT		(4U)
#define VIN_SYNC_M0_HB_SHIFT		(0U)

#define VIN_SYNC_M0_SB_MASK			((u32)0x3U << VIN_SYNC_M0_SB_SHIFT)
#define VIN_SYNC_M0_PSL_MASK		((u32)0x3U << VIN_SYNC_M0_PSL_SHIFT)
#define VIN_SYNC_M0_FP_MASK			((u32)0xFU << VIN_SYNC_M0_FP_SHIFT)
#define VIN_SYNC_M0_VB_MASK			((u32)0xFU << VIN_SYNC_M0_VB_SHIFT)
#define VIN_SYNC_M0_HB_MASK			((u32)0xFU << VIN_SYNC_M0_HB_SHIFT)

/*
 * VIN Sync Misc. 1 Register
 */
#define VIN_SYNC_M1_PT_SHIFT		(16U)
#define VIN_SYNC_M1_PS_SHIFT		(8U)
#define VIN_SYNC_M1_PF_SHIFT		(0U)

#define VIN_SYNC_M1_PT_MASK		((u32)0xFFU << VIN_SYNC_M1_PT_SHIFT)
#define VIN_SYNC_M1_PS_MASK		((u32)0xFFU << VIN_SYNC_M1_PS_SHIFT)
#define VIN_SYNC_M1_PF_MASK		((u32)0xFFU << VIN_SYNC_M1_PF_SHIFT)

/*
 * VIN Size Register
 */
#define VIN_SIZE_HEIGHT_SHIFT		(16U)
#define VIN_SIZE_WIDTH_SHIFT		(0U)

#define VIN_SIZE_HEIGHT_MASK		((u32)0xFFFFU << VIN_SIZE_HEIGHT_SHIFT)
#define VIN_SIZE_WIDTH_MASK			((u32)0xFFFFU << VIN_SIZE_WIDTH_SHIFT)

/*
 * VIN Offset Register
 */
#define VIN_OFFS_OFS_HEIGHT_SHIFT		(16U)
#define VIN_OFFS_OFS_WIDTH_SHIFT		(0U)

#define VIN_OFFS_OFS_HEIGHT_MASK ((u32)0xFFFFU << VIN_OFFS_OFS_HEIGHT_SHIFT)
#define VIN_OFFS_OFS_WIDTH_MASK  ((u32)0xFFFFU << VIN_OFFS_OFS_WIDTH_SHIFT)

/*
 * VIN Offset in Interlaced Register
 */
#define VIN_OFFS_INTL_OFS_HEIGHT_SHIFT		(16U)

#define VIN_OFFS_INTL_OFS_HEIGHT_MASK ((u32)0xFFFFU << VIN_OFFS_OFS_HEIGHT_SHIFT)

/*
 * VIN Crop Size Register
 */
#define VIN_CROP_SIZE_HEIGHT_SHIFT		(16U)
#define VIN_CROP_SIZE_WIDTH_SHIFT		(0U)

#define VIN_CROP_SIZE_HEIGHT_MASK ((u32)0xFFFFU << VIN_CROP_SIZE_HEIGHT_SHIFT)
#define VIN_CROP_SIZE_WIDTH_MASK ((u32)0xFFFFU << VIN_CROP_SIZE_WIDTH_SHIFT)

/*
 * VIN Crop Offset Register
 */
#define VIN_CROP_OFFS_OFS_HEIGHT_SHIFT		(16U)
#define VIN_CROP_OFFS_OFS_WIDTH_SHIFT		(0U)

#define VIN_CROP_OFFS_OFS_HEIGHT_MASK \
	((u32)0xFFFFU << VIN_CROP_OFFS_OFS_HEIGHT_SHIFT)
#define VIN_CROP_OFFS_OFS_WIDTH_MASK \
	((u32)0xFFFFU << VIN_CROP_OFFS_OFS_WIDTH_SHIFT)

/*
 * VIN Monitor Clear Register
 */
#define VIN_MON_CLR_CLR_SHIFT (0U)

#define VIN_MON_CLR_CLR_MASK ((u32)0x1U << VIN_MON_CLR_CLR_SHIFT)

/*
 * VIN Monitor for HSync Register
 */
#define VIN_MON_HS_MAX_SHIFT	(16U)
#define VIN_MON_HS_CNT_SHIFT	(0U)

#define VIN_MON_HS_MAX_MASK ((u32)0xFFFFU << VIN_MON_HS_MAX_SHIFT)
#define VIN_MON_HS_CNT_MASK ((u32)0xFFFFU << VIN_MON_HS_CNT_SHIFT)

/*
 * VIN Monitor for DataEnable Register
 */
#define VIN_MON_DE_MAX_SHIFT (16U)
#define VIN_MON_DE_CNT_SHIFT (0U)

#define VIN_MON_DE_MAX_MASK ((u32)0xFFFFU << VIN_MON_DE_MAX_SHIFT)
#define VIN_MON_DE_CNT_MASK ((u32)0xFFFFU << VIN_MON_DE_CNT_SHIFT)

/*
 * VIN Monitor for LineCounter Register
 */
#define VIN_MON_LCNT_MAX_SHIFT (16U)
#define VIN_MON_LCNT_CNT_SHIFT (0U)

#define VIN_MON_LCNT_MAX_MASK ((u32)0xFFFFU << VIN_MON_LCNT_MAX_SHIFT)
#define VIN_MON_LCNT_CNT_MASK ((u32)0xFFFFU << VIN_MON_LCNT_CNT_SHIFT)

/*
 * VIN Monitor for Vertical Sync Counter Register
 */
#define VIN_MON_VSCNT_CNT_SHIFT (0U)

#define VIN_MON_VSCNT_CNT_MASK ((u32)0xFFFFFFFFU << VIN_MON_VSCNT_CNT_SHIFT)

/*
 * VIN Monitor for Vertical Sync Max Register
 */
#define VIN_MON_VSCNT_MAX_SHIFT (0U)

#define VIN_MON_VSCNT_MAX_MASK ((u32)0xFFFFFFFFU << VIN_MON_VSCNT_MAX_SHIFT)

/*
 * VIN Look-up table control Register
 */
#define VIN_LUT_CTRL_IND_SHIFT			(0U)

#define VIN_LUT_CTRL_IND_MASK			((u32)0x3U << VIN_LUT_CTRL_IND_SHIFT)

/*
 * VIN Interrupt Register
 */
#define VIN_INT_INTEN_SHIFT			(31U)
#define VIN_INT_MINVS_SHIFT			(19U)
#define VIN_INT_MVS_SHIFT			(18U)
#define VIN_INT_MEOF_SHIFT			(17U)
#define VIN_INT_MUPD_SHIFT			(16U)
#define VIN_INT_FS_SHIFT			(11U)
#define VIN_INT_INVS_SHIFT			(3U)
#define VIN_INT_VS_SHIFT			(2U)
#define VIN_INT_EOF_SHIFT			(1U)
#define VIN_INT_UPD_SHIFT			(0U)

#define VIN_INT_INTEN_MASK			((u32)0x1U << VIN_INT_INTEN_SHIFT)
#define VIN_INT_MINVS_MASK			((u32)0x1U << VIN_INT_MINVS_SHIFT)
#define VIN_INT_MVS_MASK			((u32)0x1U << VIN_INT_MVS_SHIFT)
#define VIN_INT_MEOF_MASK			((u32)0x1U << VIN_INT_MEOF_SHIFT)
#define VIN_INT_MUPD_MASK			((u32)0x1U << VIN_INT_MUPD_SHIFT)
#define VIN_INT_FS_MASK				((u32)0x1U << VIN_INT_FS_SHIFT)
#define VIN_INT_INVS_MASK			((u32)0x1U << VIN_INT_INVS_SHIFT)
#define VIN_INT_VS_MASK				((u32)0x1U << VIN_INT_VS_SHIFT)
#define VIN_INT_EOF_MASK			((u32)0x1U << VIN_INT_EOF_SHIFT)
#define VIN_INT_UPD_MASK			((u32)0x1U << VIN_INT_UPD_SHIFT)

/*
 * VIN Look-up Table initialize Register
 */
#define VIN_LUT_K_C_VALUE_4K_PLUS_3_SHIFT	(24U)
#define VIN_LUT_K_C_VALUE_4K_PLUS_2_SHIFT	(16U)
#define VIN_LUT_K_C_VALUE_4K_PLUS_1_SHIFT	(8U)
#define VIN_LUT_K_C_VALUE_4K_PLUS_0_SHIFT	(0U)

#define VIN_LUT_K_C_VALUE_4K_PLUS_3_MASK \
				((u32)0xFFU << VIN_LUT_K_C_VALUE_4K_PLUS_3_SHIFT)
#define VIN_LUT_K_C_VALUE_4K_PLUS_2_MASK \
				((u32)0xFFU << VIN_LUT_K_C_VALUE_4K_PLUS_2_SHIFT)
#define VIN_LUT_K_C_VALUE_4K_PLUS_1_MASK \
				((u32)0xFFU << VIN_LUT_K_C_VALUE_4K_PLUS_1_SHIFT)
#define VIN_LUT_K_C_VALUE_4K_PLUS_0_MASK \
				((u32)0xFFU << VIN_LUT_K_C_VALUE_4K_PLUS_0_SHIFT)

/*
 * Register offset
 */
#define VD_CTRL			((u32)0x00U)
#define VD_BLANK0		((u32)0x04U)
#define VD_BLANK1		((u32)0x08U)
#define VD_MISC			((u32)0x0CU)
#define VD_STAT			((u32)0x10U)

/*
 * VIN_DEMUX Control Register
 */
#define VD_CTRL_SEL3_SHIFT		(28U)
#define VD_CTRL_SEL2_SHIFT		(24U)
#define VD_CTRL_SEL1_SHIFT		(20U)
#define VD_CTRL_SEL0_SHIFT		(16U)
#define VD_CTRL_CM_SHIFT		(8U)
#define VD_CTRL_BS_SHIFT		(4U)
#define VD_CTRL_EN_SHIFT		(0U)

#define VD_CTRL_SEL3_MASK		((u32)0x7U << VD_CTRL_SEL3_SHIFT)
#define VD_CTRL_SEL2_MASK		((u32)0x7U << VD_CTRL_SEL2_SHIFT)
#define VD_CTRL_SEL1_MASK		((u32)0x7U << VD_CTRL_SEL1_SHIFT)
#define VD_CTRL_SEL0_MASK		((u32)0x7U << VD_CTRL_SEL0_SHIFT)
#define VD_CTRL_CM_MASK			((u32)0x7U << VD_CTRL_CM_SHIFT)
#define VD_CTRL_BS_MASK			((u32)0x3U << VD_CTRL_BS_SHIFT)
#define VD_CTRL_EN_MASK			((u32)0x1U << VD_CTRL_EN_SHIFT)

/*
 * VIN_DEMUX BLANK0 Register
 */
#define VD_BLANK0_SB_SHIFT		(18U)
#define VD_BLANK0_PSL_SHIFT		(16U)
#define VD_BLANK0_FP_SHIFT		(8U)
#define VD_BLANK0_VB_SHIFT		(4U)
#define VD_BLANK0_HB_SHIFT		(0U)

#define VD_BLANK0_SB_MASK		((u32)0x3U << VD_BLANK0_SB_SHIFT)
#define VD_BLANK0_PSL_MASK		((u32)0x3U << VD_BLANK0_PSL_SHIFT)
#define VD_BLANK0_FP_MASK		((u32)0xFU << VD_BLANK0_FP_SHIFT)
#define VD_BLANK0_VB_MASK		((u32)0xFU << VD_BLANK0_VB_SHIFT)
#define VD_BLANK0_HB_MASK		((u32)0xFU << VD_BLANK0_HB_SHIFT)

/*
 * VIN_DEMUX BLANK1 Register
 */
#define VD_BLANK1_PT_SHIFT		(16U)
#define VD_BLANK1_PS_SHIFT		(8U)
#define VD_BLANK1_PF_SHIFT		(0U)

#define VD_BLANK1_PT_MASK		((u32)0xFFU << VD_BLANK1_PT_SHIFT)
#define VD_BLANK1_PS_MASK		((u32)0xFFU << VD_BLANK1_PS_SHIFT)
#define VD_BLANK1_PF_MASK		((u32)0xFFU << VD_BLANK1_PF_SHIFT)

/*
 * VIN_DEMUX Miscellaneous Register
 */
#define VD_MISC_DSEL3_SHIFT		(12U)
#define VD_MISC_DSEL2_SHIFT		(8U)
#define VD_MISC_DSEL1_SHIFT		(4U)
#define VD_MISC_DSEL0_SHIFT		(0U)

#define VD_MISC_DSEL3_MASK		((u32)0x7U << VD_MISC_DSEL3_SHIFT)
#define VD_MISC_DSEL2_MASK		((u32)0x7U << VD_MISC_DSEL2_SHIFT)
#define VD_MISC_DSEL1_MASK		((u32)0x7U << VD_MISC_DSEL1_SHIFT)
#define VD_MISC_DSEL0_MASK		((u32)0x7U << VD_MISC_DSEL0_SHIFT)

/*
 * VIN_DEMUX Status Register
 */
#define VD_STS_IDX3_SHIFT		(12U)
#define VD_STS_IDX2_SHIFT		(8U)
#define VD_STS_IDX1_SHIFT		(4U)
#define VD_STS_IDX0_SHIFT		(0U)

#define VD_STS_IDX3_MASK		((u32)0x7U << VD_STS_IDX3_SHIFT)
#define VD_STS_IDX2_MASK		((u32)0x7U << VD_STS_IDX2_SHIFT)
#define VD_STS_IDX1_MASK		((u32)0x7U << VD_STS_IDX1_SHIFT)
#define VD_STS_IDX0_MASK		((u32)0x7U << VD_STS_IDX0_SHIFT)


/* Interface APIs. */
extern void VIOC_VIN_SetSyncPolarity(void __iomem *reg,
	unsigned int hs_active_low, unsigned int vs_active_low,
	unsigned int field_bfield_low, unsigned int de_active_low,
	unsigned int gen_field_en, unsigned int pxclk_pol);
extern void VIOC_VIN_SetCtrl(void __iomem *reg,
	unsigned int conv_en, unsigned int hsde_connect_en,
	unsigned int vs_mask, unsigned int fmt, unsigned int data_order);
extern void VIOC_VIN_SetInterlaceMode(void __iomem *reg,
	unsigned int intl_en, unsigned int intpl_en);
extern void VIOC_VIN_SetCaptureModeEnable(
	void __iomem *reg, unsigned int cap_en);
extern void VIOC_VIN_SetFrameSkipNumber(void __iomem *reg,
	unsigned int skip);
extern void VIOC_VIN_SetEnable(void __iomem *reg,
	unsigned int vin_en);
extern void VIOC_VIN_SetImageSize(void __iomem *reg,
	unsigned int width, unsigned int height);
extern void VIOC_VIN_SetImageOffset(void __iomem *reg,
	unsigned int offs_width, unsigned int offs_height,
	unsigned int offs_height_intl);
extern void VIOC_VIN_SetImageCropSize(void __iomem *reg,
	unsigned int width, unsigned int height);
extern void VIOC_VIN_SetImageCropOffset(void __iomem *reg,
	unsigned int offs_width, unsigned int offs_height);
extern void VIOC_VIN_SetY2RMode(void __iomem *reg,
	unsigned int y2r_mode);
extern void VIOC_VIN_SetY2REnable(void __iomem *reg,
	unsigned int y2r_en);
extern void VIOC_VIN_SetLUT(void __iomem *reg,
	const unsigned int *pLUT);
extern void VIOC_VIN_SetLUTEnable(void __iomem *reg,
	unsigned int lut0_en, unsigned int lut1_en, unsigned int lut2_en);
extern unsigned int VIOC_VIN_IsEnable(const void __iomem *reg);

extern void VIOC_VIN_SetDemuxPort(void __iomem *reg,
	unsigned int p0, unsigned int p1, unsigned int p2, unsigned int p3);
extern void VIOC_VIN_SetDemuxClock(void __iomem *reg,
	unsigned int mode);
extern void VIOC_VIN_SetDemuxEnable(void __iomem *reg,
	unsigned int enable);
extern void VIOC_VIN_SetSEEnable(void __iomem *reg,
	unsigned int se);
extern void VIOC_VIN_SetFlushBufferEnable(
	void __iomem *reg, unsigned int fvs);
extern void VIOC_VIN_SetIreqMask(void __iomem *reg,
	unsigned int mask, unsigned int set);
extern void VIOC_VIN_GetStatus(const void __iomem *reg,
	unsigned int *status);

extern void VIOC_VIN_ClearAllMonitorCounters(void __iomem *reg);
extern unsigned int VIOC_VIN_GetHSyncMax(const void __iomem *reg);
extern unsigned int VIOC_VIN_GetHSyncCounter(const void __iomem *reg);
extern unsigned int VIOC_VIN_GetDataEnableMax(const void __iomem *reg);
extern unsigned int VIOC_VIN_GetDataEnableCounter(const void __iomem *reg);
extern unsigned int VIOC_VIN_GetLineCountMax(const void __iomem *reg);
extern unsigned int VIOC_VIN_GetLineCountCounter(const void __iomem *reg);
extern unsigned int VIOC_VIN_GetVSyncMax(const void __iomem *reg);
extern unsigned int VIOC_VIN_GetVSyncCounter(const void __iomem *reg);

extern void __iomem *VIOC_VIN_GetAddress(unsigned int Num);
#endif
