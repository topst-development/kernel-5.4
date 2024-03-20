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
#ifndef VIOC_RDMA_H
#define	VIOC_RDMA_H

#define VIOC_RDMA_STAT_ICFG            0x00000001U
#define VIOC_RDMA_STAT_IEOFR           0x00000002U
#define VIOC_RDMA_STAT_IEOFF           0x00000004U
#define VIOC_RDMA_STAT_IUPDD           0x00000008U
#define VIOC_RDMA_STAT_IEOFFW          0x00000010U
#define VIOC_RDMA_STAT_ITOPR           0x00000020U
#define VIOC_RDMA_STAT_IBOTR           0x00000040U
#define VIOC_RDMA_STAT_ALL             0x0000007FU

#define VIOC_RDMA_IREQ_ICFG_MASK       0x00000001U
#define VIOC_RDMA_IREQ_IEOFR_MASK      0x00000002U
#define VIOC_RDMA_IREQ_IEOFF_MASK      0x00000004U
#define VIOC_RDMA_IREQ_IUPDD_MASK      0x00000008U
#define VIOC_RDMA_IREQ_IEOFFW_MASK     0x00000010U
#define VIOC_RDMA_IREQ_ITOPR_MASK      0x00000020U
#define VIOC_RDMA_IREQ_IBOTR_MASK      0x00000040U
#define VIOC_RDMA_IREQ_ALL_MASK        0x0000007FU

/*
 * register offset
 */
#define RDMACTRL         0x00U
#define RDMAPTS          0x04U
#define	RDMASIZE         0x08U
#define	RDMABASE0        0x0CU
#define RDMACADDR        0x10U
#define	RDMABASE1        0x14U
#define	RDMABASE2        0x18U
#define RDMAOFFS         0x1CU
#define RDMASCALE        0x20U
#define RDMAALPHA        0x24U
#define RDMASTAT         0x28U
#define	RDMAIRQMSK       0x2CU
#define RDMASBASE0       0x30U
#define RDMA_RBASE0      0x34U
#define RDMA_RBASE1      0x38U
#define RDMA_RBASE2      0x3CU

/*
 * RDMA Control Register
 */
#define RDMACTRL_INTL_SHIFT   (31U) // Interlaced image indication
#define RDMACTRL_BFMD_SHIFT   (30U) // BFIELD Mode
#define RDMACTRL_BF_SHIFT     (29U) // Field indication
#define RDMACTRL_IEN_SHIFT    (28U) // Image Enable
#define RDMACTRL_STRM_SHIFT   (27U) // Streaming Mode
#define RDMACTRL_3DM_SHIFT    (25U) // 3D Mode Type
#define RDMACTRL_ASEL_SHIFT   (24U) // Pixel Alpha Select
#define RDMACTRL_UVI_SHIFT    (23U) // UV interpolation
#define RDMACTRL_R2YMD_SHIFT  (18U) // R2Y Converter Mode
#define RDMACTRL_R2Y_SHIFT    (17U) // R2Y Converter Enable
#define RDMACTRL_UPD_SHIFT    (16U) // Update
#define RDMACTRL_PD_SHIFT     (15U) // Padding option
#define RDMACTRL_SWAP_SHIFT   (12U) // RGB Swap
#define RDMACTRL_AEN_SHIFT    (11U) // Alpha Enable
#define RDMACTRL_Y2RMD_SHIFT  (9U)  // Y2R Converter Mode
#define RDMACTRL_Y2R_SHIFT    (8U)  // Y2R Converter Enable
#define RDMACTRL_BR_SHIFT     (7U)  // Bit Reverse
#define RDMACTRL_FMT_SHIFT    (0U)  // Image Format

#define RDMACTRL_INTL_MASK   ((u32)0x1U << RDMACTRL_INTL_SHIFT)
#define RDMACTRL_BFMD_MASK   ((u32)0x1U << RDMACTRL_BFMD_SHIFT)
#define RDMACTRL_BF_MASK     ((u32)0x1U << RDMACTRL_BF_SHIFT)
#define RDMACTRL_IEN_MASK    ((u32)0x1U << RDMACTRL_IEN_SHIFT)
#define RDMACTRL_STRM_MASK   ((u32)0x1U << RDMACTRL_STRM_SHIFT)
#define RDMACTRL_3DM_MASK    ((u32)0x3U << RDMACTRL_3DM_SHIFT)
#define RDMACTRL_ASEL_MASK   ((u32)0x1U << RDMACTRL_ASEL_SHIFT)
#define RDMACTRL_UVI_MASK    ((u32)0x1U << RDMACTRL_UVI_SHIFT)
#define RDMACTRL_R2YMD_MASK  ((u32)0x3U << RDMACTRL_R2YMD_SHIFT)
#define RDMACTRL_R2Y_MASK    ((u32)0x1U << RDMACTRL_R2Y_SHIFT)
#define RDMACTRL_UPD_MASK    ((u32)0x1U << RDMACTRL_UPD_SHIFT)
#define RDMACTRL_PD_MASK     ((u32)0x1U << RDMACTRL_PD_SHIFT)
#define RDMACTRL_SWAP_MASK   ((u32)0x7U << RDMACTRL_SWAP_SHIFT)
#define RDMACTRL_AEN_MASK    ((u32)0x1U << RDMACTRL_AEN_SHIFT)
#define RDMACTRL_Y2RMD_MASK  ((u32)0x3U << RDMACTRL_Y2RMD_SHIFT)
#define RDMACTRL_Y2R_MASK    ((u32)0x1U << RDMACTRL_Y2R_SHIFT)
#define RDMACTRL_BR_MASK     ((u32)0x1U << RDMACTRL_BR_SHIFT)
#define RDMACTRL_FMT_MASK    ((u32)0x1FU << RDMACTRL_FMT_SHIFT)

/*
 * RDMA Image PTS Register
 */
#define RDMAPTS_BOTPTS_SHIFT (16U) // Bottom field presentation time
#define RDMAPTS_TOPPTS_SHIFT (0U)  // Top field presentation time

#define	RDMAPTS_BOTPTS_MASK ((u32)0xFFFFU << RDMAPTS_BOTPTS_SHIFT)
#define	RDMAPTS_TOPPTS_MASK ((u32)0xFFFFU << RDMAPTS_TOPPTS_SHIFT)

/*
 * Image Size Information Register
 */
#define RDMASIZE_HEIGHT_SHIFT (16U) // height information
#define RDMASIZE_WIDTH_SHIFT  (0U)  // width information

#define RDMASIZE_HEIGHT_MASK ((u32)0x1FFFU << RDMASIZE_HEIGHT_SHIFT)
#define RDMASIZE_WIDTH_MASK  ((u32)0x1FFFU << RDMASIZE_WIDTH_SHIFT)

/*
 * Base Address for Each Image
 */
#define RDMABASE0_BASE0_SHIFT (0U)

#define RDMABASE0_BASE0_MASK ((u32)0xFFFFFFFFU << RDMABASE0_BASE0_SHIFT)

/*
 * Current Address for Each Image
 */
#define RDMACADDR_CURR_SHIFT (0U)

#define RDMACADDR_CURR_MASK ((u32)0xFFFFFFFFU << RDMACADDR_CURR_SHIFT)

/*
 * 2nd Base Address for Each Image
 */
#define RDMABASE1_BASE1_SHIFT (0U)

#define RDMABASE1_BASE1_MASK ((u32)0xFFFFFFFFU << RDMABASE1_BASE1_SHIFT)

/*
 * 3rd Base Address for Each Image
 */
#define RDMABASE2_BASE2_SHIFT (0U)

#define RDMABASE2_BASE2_MASK ((u32)0xFFFFFFFFU << RDMABASE2_BASE2_SHIFT)

/*
 * Offset Information Register
 */
#define RDMAOFFS_OFFSET1_SHIFT (16U)
#define RDMAOFFS_OFFSET0_SHIFT (0U)

#define RDMAOFFS_OFFSET1_MASK ((u32)0xFFFFU << RDMAOFFS_OFFSET1_SHIFT)
#define RDMAOFFS_OFFSET0_MASK ((u32)0xFFFFU << RDMAOFFS_OFFSET0_SHIFT)

/*
 * SCALE Registers for each Image
 */
#define RDMASCALE_ISSUE_SHIFT   (20U) // Command Issue count
#define RDMASCALE_YSCALE_SHIFT (16U) // Image Scaling in Vertical
#define RDMASCALE_XSCALE_SHIFT  (0U) // Image Scaling in Horizontal

#define RDMASCALE_ISSUE_MASK   ((u32)0xFFFU << RDMASCALE_ISSUE_SHIFT)
#define RDMASCALE_YSCALE_MASK ((u32)0x7U << RDMASCALE_YSCALE_SHIFT)
#define RDMASCALE_XSCALE_MASK  ((u32)0x7U << RDMASCALE_XSCALE_SHIFT)

/*
 * Alpha Information Register for Each Image
 */
#define RDMAALPHA_A1_SHIFT (16U)
#define RDMAALPHA_A0_SHIFT (0U)

#define RDMAALPHA_A1_MASK ((u32)0xFFU << RDMAALPHA_A1_SHIFT)
#define RDMAALPHA_A0_MASK ((u32)0xFFU << RDMAALPHA_A0_SHIFT)

/*
 * RDMA Status Register
 */
#define RDMASTAT_SFDLY_SHIFT (28U) // Delayred frame number
#define RDMASTAT_SBF_SHIFT   (20U) // Bfield status
#define RDMASTAT_SDEOF_SHIFT (19U) // device eof
#define RDMASTAT_SEOFW_SHIFT (18U) // rdma eof wait
#define RDMASTAT_SBOTR_SHIFT (17U) // bottom field ready
#define RDMASTAT_STOPR_SHIFT (16U) // top field ready
#define RDMASTAT_BOTR_SHIFT  (6U)  // bottom ready
#define RDMASTAT_TOPR_SHIFT  (5U)  // top ready
#define RDMASTAT_EOFW_SHIFT  (4U)  // eof-wait rising
#define RDMASTAT_UPDD_SHIFT  (3U)  // update done
#define RDMASTAT_EOFF_SHIFT  (2U)  // eof falling
#define RDMASTAT_EOFR_SHIFT  (1U)  // eof rising
#define RDMASTAT_CFG_SHIFT   (0U)  // configuration update

#define RDMASTAT_SFDLY_MASK ((u32)0xFU << RDMASTAT_SFDLY_SHIFT)
#define RDMASTAT_SBF_MASK   ((u32)0x1U << RDMASTAT_SBF_SHIFT)
#define RDMASTAT_SDEOF_MASK ((u32)0x1U << RDMASTAT_SDEOF_SHIFT)
#define RDMASTAT_SEOFW_MASK ((u32)0x1U << RDMASTAT_SEOFW_SHIFT)
#define RDMASTAT_SBOTR_MASK ((u32)0x1U << RDMASTAT_SBOTR_SHIFT)
#define RDMASTAT_STOPR_MASK ((u32)0x1U << RDMASTAT_STOPR_SHIFT)
#define RDMASTAT_BOTR_MASK  ((u32)0x1U << RDMASTAT_BOTR_SHIFT)
#define RDMASTAT_TOPR_MASK  ((u32)0x1U << RDMASTAT_TOPR_SHIFT)
#define RDMASTAT_EOFW_MASK  ((u32)0x1U << RDMASTAT_EOFW_SHIFT)
#define RDMASTAT_UPDD_MASK  ((u32)0x1U << RDMASTAT_UPDD_SHIFT)
#define RDMASTAT_EOFF_MASK  ((u32)0x1U << RDMASTAT_EOFF_SHIFT)
#define RDMASTAT_EOFR_MASK  ((u32)0x1U << RDMASTAT_EOFR_SHIFT)
#define RDMASTAT_CFG_MASK   ((u32)0x1U << RDMASTAT_CFG_SHIFT)

/*
 * RDMA Interrupt Mask Register
 */
#define RDMAIRQMSK_IBOTR_SHIFT (6U) // Bottom Ready
#define RDMAIRQMSK_ITOPR_SHIFT (5U) // Top Ready
#define RDMAIRQMSK_IEOFW_SHIFT (4U) // EOF-WAIT Rising
#define RDMAIRQMSK_IUPDD_SHIFT (3U) // Register Update Done
#define RDMAIRQMSK_IEOFF_SHIFT (2U) // Device EOF Failling
#define RDMAIRQMSK_IEOFR_SHIFT (1U) // Device EOF Rising
#define RDMAIRQMSK_ICFG_SHIFT  (0U) // Configuration Update

#define RDMAIRQMSK_IBOTR_MASK ((u32)0x1U << RDMAIRQMSK_IBOTR_SHIFT)
#define RDMAIRQMSK_ITOPR_MASK ((u32)0x1U << RDMAIRQMSK_ITOPR_SHIFT)
#define RDMAIRQMSK_IEOFW_MASK ((u32)0x1U << RDMAIRQMSK_IEOFW_SHIFT)
#define RDMAIRQMSK_IUPDD_MASK ((u32)0x1U << RDMAIRQMSK_IUPDD_SHIFT)
#define RDMAIRQMSK_IEOFF_MASK ((u32)0x1U << RDMAIRQMSK_IEOFF_SHIFT)
#define RDMAIRQMSK_IEOFR_MASK ((u32)0x1U << RDMAIRQMSK_IEOFR_SHIFT)
#define RDMAIRQMSK_ICFG_MASK  ((u32)0x1U << RDMAIRQMSK_ICFG_SHIFT)
#define RDMAIRQMSK_ALL_MASK \
	(RDMAIRQMSK_IBOTR_MASK \
	| RDMAIRQMSK_ITOPR_MASK \
	| RDMAIRQMSK_IEOFW_MASK \
	| RDMAIRQMSK_IUPDD_MASK \
	| RDMAIRQMSK_IEOFF_MASK \
	| RDMAIRQMSK_IEOFR_MASK \
	| RDMAIRQMSK_ICFG_MASK)

/*
 * RDMA sync Base Address 0
 */
#define RDMASBASE0_SBADDR_SHIFT (0U)

#define RDMASBASE0_SBADDR_MASK ((u32)0xFFFFFFFFU << RDMASBASE0_SBADDR_SHIFT)

/*
 * Base Address for Each Image
 */
#define RDMA_RBASE0_BASE0_SHIFT (0U)

#define RDMA_RBASE0_BASE0_MASK ((u32)0xFFFFFFFFU << RDMA_RBASE0_BASE0_SHIFT)

/*
 * Base Address for Each Image
 */
#define RDMA_RBASE1_BASE0_SHIFT (0U)

#define RDMA_RBASE1_BASE0_MASK ((u32)0xFFFFFFFFU << RDMA_RBASE1_BASE0_SHIFT)

/*
 * Base Address for Each Image
 */
#define RDMA_RBASE2_BASE0_SHIFT (0U)

#define RDMA_RBASE2_BASE0_MASK ((u32)0xFFFFFFFFU << RDMA_RBASE2_BASE0_SHIFT)

/*
 * RDMA Crop Size for Each Images
 */
#define RDMA_CROP_SIZE_HEIGHT_SHIFT (16U) // height for Crop
#define RDMA_CROP_SIZE_WIDTH_SHIFT  (0U)  // width for Crop

#define RDMA_CROP_SIZE_HEIGHT_MASK ((u32)0x1FFFU << RDMA_CROP_SIZE_HEIGHT_SHIFT)
#define RDMA_CROP_SIZE_WIDTH_MASK  ((u32)0x1FFFU << RDMA_CROP_SIZE_WIDTH_SHIFT)

/* Interface APIs */
extern void VIOC_RDMA_SetImageUpdate(void __iomem *reg);
extern void VIOC_RDMA_SetImageEnable(void __iomem *reg);
extern void VIOC_RDMA_SetImageDisable(void __iomem *reg);
extern void VIOC_RDMA_SetImageDisableNW(
	void __iomem *reg);
extern void VIOC_RDMA_GetImageEnable(
	const void __iomem *reg, unsigned int *enable);
extern void VIOC_RDMA_SetImageFormat(
	void __iomem *reg, unsigned int nFormat);
extern void VIOC_RDMA_SetImageRGBSwapMode(
	void __iomem *reg, unsigned int rgb_mode);
extern void VIOC_RDMA_SetImageAlphaEnable(
	void __iomem *reg, unsigned int enable);
extern void VIOC_RDMA_GetImageAlphaEnable(
	const void __iomem *reg, unsigned int *enable);
extern void VIOC_RDMA_SetImageAlphaSelect(
	void __iomem *reg, unsigned int select);
extern void VIOC_RDMA_SetImageY2RMode(
	void __iomem *reg, unsigned int y2r_mode);
extern void VIOC_RDMA_SetImageY2REnable(
	void __iomem *reg, unsigned int enable);
extern void VIOC_RDMA_SetImageR2YMode(
	void __iomem *reg, unsigned int r2y_mode);
extern void VIOC_RDMA_SetImageR2YEnable(
	void __iomem *reg, unsigned int enable);
extern void VIOC_RDMA_GetImageR2YEnable(
	const void __iomem *reg, unsigned int *enable);
extern void VIOC_RDMA_SetImageAlpha(
	void __iomem *reg,
	unsigned int nAlpha0, unsigned int nAlpha1);
extern void VIOC_RDMA_GetImageAlpha(
	const void __iomem *reg,
	unsigned int *nAlpha0, unsigned int *nAlpha1);
extern void VIOC_RDMA_SetImageUVIEnable(
	void __iomem *reg, unsigned int enable);
extern void VIOC_RDMA_SetImage3DMode(
	void __iomem *reg, unsigned int mode_3d);
extern void VIOC_RDMA_SetImageSize(
	void __iomem *reg, unsigned int sw, unsigned int sh);
extern void VIOC_RDMA_GetImageSize(
	const void __iomem *reg, unsigned int *sw, unsigned int *sh);
extern void VIOC_RDMA_SetImageBase(
	void __iomem *reg,
	unsigned int nBase0, unsigned int nBase1, unsigned int nBase2);
extern void VIOC_RDMA_SetImageRBase(
	void __iomem *reg,
	unsigned int nBase0, unsigned int nBase1, unsigned int nBase2);
extern void VIOC_RDMA_SetImageOffset(
	void __iomem *reg,
	unsigned int imgFmt, unsigned int imgWidth);
extern void VIOC_RDMA_SetImageScale(
	void __iomem *reg,
	unsigned int scaleX, unsigned int scaleY);
extern void VIOC_RDMA_SetImageBfield(
	void __iomem *reg, unsigned int bfield);
extern void VIOC_RDMA_SetImageBFMD(
	void __iomem *reg, unsigned int bfmd);
extern void VIOC_RDMA_SetImageIntl(
	void __iomem *reg, unsigned int intl_en);
extern void VIOC_RDMA_SetStatus(
	void __iomem *reg, unsigned int mask);
extern void VIOC_RDMA_SetIssue(
	void __iomem *reg,
	unsigned int burst_length, unsigned int issue_cnt);
extern void VIOC_RDMA_SetIreqMask(
	void __iomem *reg, unsigned int mask, unsigned int set);
extern unsigned int VIOC_RDMA_GetStatus(const void __iomem *reg);
extern unsigned int VIOC_RDMA_Get_CAddress(const void __iomem *reg);
extern void __iomem *VIOC_RDMA_GetAddress(unsigned int vioc_id);
extern int VIOC_RDMA_IsVRDMA(unsigned int vioc_id);
extern int VIOC_RDMA_GetImageNum(unsigned int vioc_id);
extern void VIOC_RDMA_DUMP(const void __iomem *reg, unsigned int vioc_id);
#ifdef CONFIG_VIOC_DOLBY_VISION_CERTIFICATION_TEST_UI // No UI-Blending
extern void VIOC_RDMA_PreventEnable_for_UI(
	char no_update, char disabled);
#endif
#endif
