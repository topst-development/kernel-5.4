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
#ifndef __VIOC_RDMA_H__
#define __VIOC_RDMA_H__

#ifdef CONFIG_VIOC_DOLBY_VISION_CERTIFICATION_TEST
#define CONFIG_VIOC_DOLBY_VISION_CERTIFICATION_TEST_UI
#endif

#define VIOC_RDMA_STAT_ICFG 0x00000001UL
#define VIOC_RDMA_STAT_IEOFR 0x00000002UL
#define VIOC_RDMA_STAT_IEOFF 0x00000004UL
#define VIOC_RDMA_STAT_IUPDD 0x00000008UL
#define VIOC_RDMA_STAT_IEOFFW 0x00000010UL
#define VIOC_RDMA_STAT_ITOPR 0x00000020UL
#define VIOC_RDMA_STAT_IBOTR 0x00000040UL
#define VIOC_RDMA_STAT_ALL 0x0000007FUL

#define VIOC_RDMA_IREQ_ICFG_MASK 0x00000001UL
#define VIOC_RDMA_IREQ_IEOFR_MASK 0x00000002UL
#define VIOC_RDMA_IREQ_IEOFF_MASK 0x00000004UL
#define VIOC_RDMA_IREQ_IUPDD_MASK 0x00000008UL
#define VIOC_RDMA_IREQ_IEOFFW_MASK 0x00000010UL
#define VIOC_RDMA_IREQ_ITOPR_MASK 0x00000020UL
#define VIOC_RDMA_IREQ_IBOTR_MASK 0x00000040UL
#define VIOC_RDMA_IREQ_ALL_MASK 0x0000007FUL

/*
 * register offset
 */
#define RDMACTRL 0x00
#define RDMAPTS 0x04
#define RDMASIZE 0x08
#define RDMABASE0 0x0C
#define RDMACADDR 0x10
#define RDMABASE1 0x14
#define RDMABASE2 0x18
#define RDMAOFFS 0x1C
#define RDMASCALE 0x20
#define RDMAALPHA 0x24
#define RDMASTAT 0x28
#define RDMAIRQMSK 0x2C
#define RDMASBASE0 0x30
#define RDMA_RBASE0 0x34
#define RDMA_RBASE1 0x38
#define RDMA_RBASE2 0x3C

/*
 * RDMA Control Register
 */
#define RDMACTRL_INTL_SHIFT (31)  // Interlaced image indication
#define RDMACTRL_BFMD_SHIFT (30)  // BFIELD Mode
#define RDMACTRL_BF_SHIFT (29)    // Field indication
#define RDMACTRL_IEN_SHIFT (28)   // Image Enable
#define RDMACTRL_STRM_SHIFT (27)  // Streaming Mode
#define RDMACTRL_3DM_SHIFT (25)   // 3D Mode Type
#define RDMACTRL_ASEL_SHIFT (24)  // Pixel Alpha Select
#define RDMACTRL_UVI_SHIFT (23)   // UV interpolation
#define RDMACTRL_R2YMD_SHIFT (18) // R2Y Converter Mode
#define RDMACTRL_R2Y_SHIFT (17)   // R2Y Converter Enable
#define RDMACTRL_UPD_SHIFT (16)   // Update
#define RDMACTRL_PD_SHIFT (15)    // Padding option
#define RDMACTRL_SWAP_SHIFT (12)  // RGB Swap
#define RDMACTRL_AEN_SHIFT (11)   // Alpha Enable
#define RDMACTRL_Y2RMD_SHIFT (9)  // Y2R Converter Mode
#define RDMACTRL_Y2R_SHIFT (8)    // Y2R Converter Enable
#define RDMACTRL_BR_SHIFT (7)     // Bit Reverse
#define RDMACTRL_FMT_SHIFT (0)    // Image Format

#define RDMACTRL_INTL_MASK (0x1 << RDMACTRL_INTL_SHIFT)
#define RDMACTRL_BFMD_MASK (0x1 << RDMACTRL_BFMD_SHIFT)
#define RDMACTRL_BF_MASK (0x1 << RDMACTRL_BF_SHIFT)
#define RDMACTRL_IEN_MASK (0x1 << RDMACTRL_IEN_SHIFT)
#define RDMACTRL_STRM_MASK (0x1 << RDMACTRL_STRM_SHIFT)
#define RDMACTRL_3DM_MASK (0x3 << RDMACTRL_3DM_SHIFT)
#define RDMACTRL_ASEL_MASK (0x1 << RDMACTRL_ASEL_SHIFT)
#define RDMACTRL_UVI_MASK (0x1 << RDMACTRL_UVI_SHIFT)
#define RDMACTRL_R2YMD_MASK (0x3 << RDMACTRL_R2YMD_SHIFT)
#define RDMACTRL_R2Y_MASK (0x1 << RDMACTRL_R2Y_SHIFT)
#define RDMACTRL_UPD_MASK (0x1 << RDMACTRL_UPD_SHIFT)
#define RDMACTRL_PD_MASK (0x1 << RDMACTRL_PD_SHIFT)
#define RDMACTRL_SWAP_MASK (0x7 << RDMACTRL_SWAP_SHIFT)
#define RDMACTRL_AEN_MASK (0x1 << RDMACTRL_AEN_SHIFT)
#define RDMACTRL_Y2RMD_MASK (0x3 << RDMACTRL_Y2RMD_SHIFT)
#define RDMACTRL_Y2R_MASK (0x1 << RDMACTRL_Y2R_SHIFT)
#define RDMACTRL_BR_MASK (0x1 << RDMACTRL_BR_SHIFT)
#define RDMACTRL_FMT_MASK (0x1F << RDMACTRL_FMT_SHIFT)

/*
 * RDMA Image PTS Register
 */
#define RDMAPTS_BOTPTS_SHIFT (16) // Bottom field presentation time
#define RDMAPTS_TOPPTS_SHIFT (0)  // Top field presentation time

#define RDMAPTS_BOTPTS_MASK (0xFFFF << RDMAPTS_BOTPTS_SHIFT)
#define RDMAPTS_TOPPTS_MASK (0xFFFF << RDMAPTS_TOPPTS_SHIFT)

/*
 * Image Size Information Register
 */
#define RDMASIZE_HEIGHT_SHIFT (16) // height information
#define RDMASIZE_WIDTH_SHIFT (0)   // width information

#define RDMASIZE_HEIGHT_MASK (0x1FFF << RDMASIZE_HEIGHT_SHIFT)
#define RDMASIZE_WIDTH_MASK (0x1FFF << RDMASIZE_WIDTH_SHIFT)

/*
 * Base Address for Each Image
 */
#define RDMABASE0_BASE0_SHIFT (0)

#define RDMABASE0_BASE0_MASK (0xFFFFFFFF << RDMABASE0_BASE0_SHIFT)

/*
 * Current Address for Each Image
 */
#define RDMACADDR_CURR_SHIFT (0)

#define RDMACADDR_CURR_MASK (0xFFFFFFFF << RDMACADDR_CURR_SHIFT)

/*
 * 2nd Base Address for Each Image
 */
#define RDMABASE1_BASE1_SHIFT (0)

#define RDMABASE1_BASE1_MASK (0xFFFFFFFF << RDMABASE1_BASE1_SHIFT)

/*
 * 3rd Base Address for Each Image
 */
#define RDMABASE2_BASE2_SHIFT (0)

#define RDMABASE2_BASE2_MASK (0xFFFFFFFF << RDMABASE2_BASE2_SHIFT)

/*
 * Offset Information Register
 */
#define RDMAOFFS_OFFSET1_SHIFT (16)
#define RDMAOFFS_OFFSET0_SHIFT (0)

#define RDMAOFFS_OFFSET1_MASK (0xFFFF << RDMAOFFS_OFFSET1_SHIFT)
#define RDMAOFFS_OFFSET0_MASK (0xFFFF << RDMAOFFS_OFFSET0_SHIFT)

/*
 * Scale Register for Each Image
 */
#define RDMASCALE_YSCALE_SHIFT (16) // Image Scaling in Vertical
#define RDMASCALE_XSCALE_SHIFT (0)  // Image Scaling in Horizontal

#define RDMASCALE_YSCALE_MASK (0x7 << RDMASCALE_YSCALE_SHIFT)
#define RDMASCALE_XSCALE_MASK (0x7 << RDMASCALE_XSCALE_SHIFT)

/*
 * Alpha Information Register for Each Image
 */
#define RDMAALPHA_A1_SHIFT (16)
#define RDMAALPHA_A0_SHIFT (0)

#define RDMAALPHA_A1_MASK (0x3FF << RDMAALPHA_A1_SHIFT)
#define RDMAALPHA_A0_MASK (0x3FF << RDMAALPHA_A0_SHIFT)

/*
 * RDMA Status Register
 */
#define RDMASTAT_SFDLY_SHIFT (28) // Delayred frame number
#define RDMASTAT_SBF_SHIFT (20)   // Bfield status
#define RDMASTAT_SDEOF_SHIFT (19) // device eof
#define RDMASTAT_SEOFW_SHIFT (18) // rdma eof wait
#define RDMASTAT_SBOTR_SHIFT (17) // bottom field ready
#define RDMASTAT_STOPR_SHIFT (16) // top field ready
#define RDMASTAT_BOTR_SHIFT (6)   // bottom ready
#define RDMASTAT_TOPR_SHIFT (5)   // top ready
#define RDMASTAT_EOFW_SHIFT (4)   // eof-wait rising
#define RDMASTAT_UPDD_SHIFT (3)   // update done
#define RDMASTAT_EOFF_SHIFT (2)   // eof falling
#define RDMASTAT_EOFR_SHIFT (1)   // eof rising
#define RDMASTAT_CFG_SHIFT (0)    // configuration update

#define RDMASTAT_SFDLY_MASK (0xF << RDMASTAT_SFDLY_SHIFT)
#define RDMASTAT_SBF_MASK (0x1 << RDMASTAT_SBF_SHIFT)
#define RDMASTAT_SDEOF_MASK (0x1 << RDMASTAT_SDEOF_SHIFT)
#define RDMASTAT_SEOFW_MASK (0x1 << RDMASTAT_SEOFW_SHIFT)
#define RDMASTAT_SBOTR_MASK (0x1 << RDMASTAT_SBOTR_SHIFT)
#define RDMASTAT_STOPR_MASK (0x1 << RDMASTAT_STOPR_SHIFT)
#define RDMASTAT_BOTR_MASK (0x1 << RDMASTAT_BOTR_SHIFT)
#define RDMASTAT_TOPR_MASK (0x1 << RDMASTAT_TOPR_SHIFT)
#define RDMASTAT_EOFW_MASK (0x1 << RDMASTAT_EOFW_SHIFT)
#define RDMASTAT_UPDD_MASK (0x1 << RDMASTAT_UPDD_SHIFT)
#define RDMASTAT_EOFF_MASK (0x1 << RDMASTAT_EOFF_SHIFT)
#define RDMASTAT_EOFR_MASK (0x1 << RDMASTAT_EOFR_SHIFT)
#define RDMASTAT_CFG_MASK (0x1 << RDMASTAT_CFG_SHIFT)

/*
 * RDMA Interrupt Mask Register
 */
#define RDMAIRQMSK_IBOTR_SHIFT (6) // Bottom Ready
#define RDMAIRQMSK_ITOPR_SHIFT (5) // Top Ready
#define RDMAIRQMSK_IEOFW_SHIFT (4) // EOF-WAIT Rising
#define RDMAIRQMSK_IUPDD_SHIFT (3) // Register Update Done
#define RDMAIRQMSK_IEOFF_SHIFT (2) // Device EOF Failling
#define RDMAIRQMSK_IEOFR_SHIFT (1) // Device EOF Rising
#define RDMAIRQMSK_ICFG_SHIFT (0)  // Configuration Update

#define RDMAIRQMSK_IBOTR_MASK (0x1 << RDMAIRQMSK_IBOTR_SHIFT)
#define RDMAIRQMSK_ITOPR_MASK (0x1 << RDMAIRQMSK_ITOPR_SHIFT)
#define RDMAIRQMSK_IEOFW_MASK (0x1 << RDMAIRQMSK_IEOFW_SHIFT)
#define RDMAIRQMSK_IUPDD_MASK (0x1 << RDMAIRQMSK_IUPDD_SHIFT)
#define RDMAIRQMSK_IEOFF_MASK (0x1 << RDMAIRQMSK_IEOFF_SHIFT)
#define RDMAIRQMSK_IEOFR_MASK (0x1 << RDMAIRQMSK_IEOFR_SHIFT)
#define RDMAIRQMSK_ICFG_MASK (0x1 << RDMAIRQMSK_ICFG_SHIFT)
#define RDMAIRQMSK_ALL_MASK                                                    \
	(RDMAIRQMSK_IBOTR_MASK | RDMAIRQMSK_ITOPR_MASK | RDMAIRQMSK_IEOFW_MASK \
	 | RDMAIRQMSK_IUPDD_MASK | RDMAIRQMSK_IEOFF_MASK                       \
	 | RDMAIRQMSK_IEOFR_MASK | RDMAIRQMSK_ICFG_MASK)
/*
 * RDMA sync Base Address 0
 */
#define RDMASBASE0_SBADDR_SHIFT (0)

#define RDMASBASE0_SBADDR_MASK (0xFFFFFFFF << RDMASBASE0_SBADDR_SHIFT)

/*
 * Base Address for Each Image
 */
#define RDMA_RBASE0_BASE0_SHIFT (0)

#define RDMA_RBASE0_BASE0_MASK (0xFFFFFFFF << RDMA_RBASE0_BASE0_SHIFT)

/*
 * Base Address for Each Image
 */
#define RDMA_RBASE1_BASE0_SHIFT (0)

#define RDMA_RBASE1_BASE0_MASK (0xFFFFFFFF << RDMA_RBASE1_BASE0_SHIFT)

/*
 * Base Address for Each Image
 */
#define RDMA_RBASE2_BASE0_SHIFT (0)

#define RDMA_RBASE2_BASE0_MASK (0xFFFFFFFF << RDMA_RBASE2_BASE0_SHIFT)

/* Interface APIs */
extern void VIOC_RDMA_SetImageConfig(void __iomem *reg);
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
extern void VIOC_RDMA_SetImageBfield(
	void __iomem *reg, unsigned int bfield);
extern void VIOC_RDMA_SetImageBFMD(
	void __iomem *reg, unsigned int bfmd);
extern void VIOC_RDMA_SetImageIntl(
	void __iomem *reg, unsigned int intl_en);
extern void VIOC_RDMA_SetStatus(
	void __iomem *reg, unsigned int mask);
extern void VIOC_RDMA_SetIreqMask(
	void __iomem *reg, unsigned int mask, unsigned int set);
extern unsigned int VIOC_RDMA_GetStatus(const void __iomem *reg);
extern unsigned int VIOC_RDMA_Get_CAddress(const void __iomem *reg);

extern void __iomem *VIOC_RDMA_GetAddress(unsigned int vioc_id);
extern int VIOC_RDMA_IsVRDMA(unsigned int vioc_id);
extern int VIOC_RDMA_GetImageNum(unsigned int vioc_id);
extern void VIOC_RDMA_DUMP(const void __iomem *reg, unsigned int vioc_id);
#endif