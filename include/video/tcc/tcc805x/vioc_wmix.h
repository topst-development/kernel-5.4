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
#ifndef VIOC_WMIX_H
#define VIOC_WMIX_H

enum VIOC_WMIX_ALPHA_SELECT_Type {
	VIOC_WMIX_ALPHA_SEL0 = 0,
	VIOC_WMIX_ALPHA_SEL1,
	VIOC_WMIX_ALPHA_SEL2,
	VIOC_WMIX_ALPHA_SEL3,
	VIOC_WMIX_ALPHA_SEL_MAX
};

enum VIOC_WMIX_ALPHA_ROPMODE_Type {
	VIOC_WMIX_ALPHA_ROP_NOTDEFINE = 0, // not defined
	VIOC_WMIX_ALPHA_ROP_GLOBAL = 2,    // Global Alpha
	VIOC_WMIX_ALPHA_ROP_PIXEL,         // Pixel Alpha
	VIOC_WMIX_ALPHA_ROP_MAX
};

enum VIOC_WMIX_ALPHA_ACON0_Type {
	VIOC_WMIX_ALPHA_ACON0_0 = 0,
	VIOC_WMIX_ALPHA_ACON0_1,
	VIOC_WMIX_ALPHA_ACON0_2,
	VIOC_WMIX_ALPHA_ACON0_3,
	VIOC_WMIX_ALPHA_ACON0_4,
	VIOC_WMIX_ALPHA_ACON0_5,
	VIOC_WMIX_ALPHA_ACON0_6,
	VIOC_WMIX_ALPHA_ACON0_7,
	VIOC_WMIX_ALPHA_ACON0_MAX
};

enum VIOC_WMIX_ALPHA_ACON1_Type {
	VIOC_WMIX_ALPHA_ACON1_0 = 0,
	VIOC_WMIX_ALPHA_ACON1_1,
	VIOC_WMIX_ALPHA_ACON1_2,
	VIOC_WMIX_ALPHA_ACON1_3,
	VIOC_WMIX_ALPHA_ACON1_4,
	VIOC_WMIX_ALPHA_ACON1_5,
	VIOC_WMIX_ALPHA_ACON1_6,
	VIOC_WMIX_ALPHA_ACON1_7,
	VIOC_WMIX_ALPHA_ACON1_MAX
};

enum VIOC_WMIX_ALPHA_CCON0_Type {
	VIOC_WMIX_ALPHA_CCON0_0 = 0,
	VIOC_WMIX_ALPHA_CCON0_1,
	VIOC_WMIX_ALPHA_CCON0_2,
	VIOC_WMIX_ALPHA_CCON0_3,
	VIOC_WMIX_ALPHA_CCON0_4,
	VIOC_WMIX_ALPHA_CCON0_5,
	VIOC_WMIX_ALPHA_CCON0_6,
	VIOC_WMIX_ALPHA_CCON0_7,
	VIOC_WMIX_ALPHA_CCON0_8,
	VIOC_WMIX_ALPHA_CCON0_9,
	VIOC_WMIX_ALPHA_CCON0_10,
	VIOC_WMIX_ALPHA_CCON0_11,
	VIOC_WMIX_ALPHA_CCON0_12,
	VIOC_WMIX_ALPHA_CCON0_13,
	VIOC_WMIX_ALPHA_CCON0_14,
	VIOC_WMIX_ALPHA_CCON0_15,
	VIOC_WMIX_ALPHA_CCON0_MAX
};

enum VIOC_WMIX_ALPHA_CCON1_Type {
	VIOC_WMIX_ALPHA_CCON1_0 = 0,
	VIOC_WMIX_ALPHA_CCON1_1,
	VIOC_WMIX_ALPHA_CCON1_2,
	VIOC_WMIX_ALPHA_CCON1_3,
	VIOC_WMIX_ALPHA_CCON1_4,
	VIOC_WMIX_ALPHA_CCON1_5,
	VIOC_WMIX_ALPHA_CCON1_6,
	VIOC_WMIX_ALPHA_CCON1_7,
	VIOC_WMIX_ALPHA_CCON1_8,
	VIOC_WMIX_ALPHA_CCON1_9,
	VIOC_WMIX_ALPHA_CCON1_10,
	VIOC_WMIX_ALPHA_CCON1_11,
	VIOC_WMIX_ALPHA_CCON1_12,
	VIOC_WMIX_ALPHA_CCON1_13,
	VIOC_WMIX_ALPHA_CCON1_14,
	VIOC_WMIX_ALPHA_CCON1_15,
	VIOC_WMIX_ALPHA_CCON1_MAX
};

#define VIOC_WMIX_ALPHA_REGION_A 0x0
#define VIOC_WMIX_ALPHA_REGION_B 0x1
#define VIOC_WMIX_ALPHA_REGION_C 0x2
#define VIOC_WMIX_ALPHA_REGION_D 0x3

#define VIOC_WMIX_TYPE_2TO2	0x0
#define VIOC_WMIX_TYPE_4TO2 0x1

/*
 * register offset
 */
#define MCTRL			(0x00U)
#define MBG				(0x04U)
#define MSIZE			(0x08U)
#define MPOS0			(0x10U)
#define MPOS1			(0x14U)
#define MPOS2			(0x18U)
#define MPOS3			(0x1CU)
#define MKEY00			(0x20U)
#define MKEY01			(0x24U)
#define MKEY10			(0x28U)
#define MKEY11			(0x2CU)
#define MKEY20			(0x30U)
#define MKEY21			(0x34U)
#define MSTS			(0x38U)
#define MIRQMSK			(0x3CU)
#define MACON0			(0x40U)
#define MCCON0			(0x44U)
#define MROPC0			(0x48U)
#define MPAT0			(0x4CU)
#define MACON1			(0x50U)
#define MCCON1			(0x54U)
#define MROPC1			(0x58U)
#define MPAT1			(0x5CU)
#define MACON2			(0x60U)
#define MCCON2			(0x64U)
#define MROPC2			(0x68U)
#define MPAT2			(0x6CU)

/*
 * WMIX Control Resiger
 */
#define MCTRL_3DMD3_SHIFT		(26U) // 3D mode for IMG3
#define MCTRL_3DMD2_SHIFT		(23U) // 3D mode for IMG2
#define MCTRL_3DMD1_SHIFT		(20U) // 3D mode for IMG1
#define MCTRL_3DMD0_SHIFT		(17U) // 3D mode for IMG0
#define MCTRL_UPD_SHIFT			(16U) // Update
#define MCTRL_3DEN3_SHIFT		(11U) // 3D mode enable in IMG3
#define MCTRL_3DEN2_SHIFT		(10U) // 3D mode enable in IMG2
#define MCTRL_3DEN1_SHIFT		(9U)  // 3D mode enable in IMG1
#define MCTRL_3DEN0_SHIFT		(8U)  // 3D mode enable in IMG0
#define MCTRL_STR_SHIFT			(6U)  // Select Mix struct
#define MCTRL_OVP_SHIFT			(0U)  // Overlay Priority

#define MCTRL_3DMD3_MASK		((u32)0x7U << MCTRL_3DMD3_SHIFT)
#define MCTRL_3DMD2_MASK		((u32)0x7U << MCTRL_3DMD2_SHIFT)
#define MCTRL_3DMD1_MASK		((u32)0x7U << MCTRL_3DMD1_SHIFT)
#define MCTRL_3DMD0_MASK		((u32)0x7U << MCTRL_3DMD0_SHIFT)
#define MCTRL_UPD_MASK			((u32)0x1U << MCTRL_UPD_SHIFT)
#define MCTRL_3DEN3_MASK		((u32)0x1U << MCTRL_3DEN3_SHIFT)
#define MCTRL_3DEN2_MASK		((u32)0x1U << MCTRL_3DEN2_SHIFT)
#define MCTRL_3DEN1_MASK		((u32)0x1U << MCTRL_3DEN1_SHIFT)
#define MCTRL_3DEN0_MASK		((u32)0x1U << MCTRL_3DEN0_SHIFT)
#define MCTRL_STR_MASK			((u32)0x3U << MCTRL_STR_SHIFT)
#define MCTRL_OVP_MASK			((u32)0x1FU << MCTRL_OVP_SHIFT)

/*
 *  WMIX Backgroud Color Register
 */
#define MBG_BG3_SHIFT			(24U) // Alpha
#define MBG_BG2_SHIFT			(16U) // Y/B
#define MBG_BG1_SHIFT			(8U)  // Cb/G
#define MBG_BG0_SHIFT			(0U)  // Cr/R

#define MBG_BG3_MASK			((u32)0xFFU << MBG_BG3_SHIFT)
#define MBG_BG2_MASK			((u32)0xFFU << MBG_BG2_SHIFT)
#define MBG_BG1_MASK			((u32)0xFFU << MBG_BG1_SHIFT)
#define MBG_BG0_MASK			((u32)0xFFU << MBG_BG0_SHIFT)

/*
 * WMIX Size Register
 */
#define MSIZE_HEIGHT_SHIFT (16U) // Image Height
#define MSIZE_WIDTH_SHIFT  (0U)  // Image Width

#define MSIZE_HEIGHT_MASK ((u32)0x1FFFU << MSIZE_HEIGHT_SHIFT)
#define MSIZE_WIDTH_MASK  ((u32)0x1FFFU << MSIZE_WIDTH_SHIFT)

/*
 * WMIX Image k Position Register
 */
#define MPOS_3DSM_SHIFT (30U) // 3D split mode
#define MPOS_YPOS_SHIFT (16U) // Y-Position in Image k
#define MPOS_XPOS_SHIFT (0U)  // X-Position in Image k

#define MPOS_3DSM_MASK ((u32)0x3U << MPOS_3DSM_SHIFT)
#define MPOS_YPOS_MASK ((u32)0x1FFFU << MPOS_YPOS_SHIFT)
#define MPOS_XPOS_MASK ((u32)0x1FFFU << MPOS_XPOS_SHIFT)

/*
 * WMIX Image k Chromak-key Register
 */
#define MKEY0_KEN_SHIFT  (31U) // Chroma-Key Enable
#define MKEY0_KRYR_SHIFT (16U) // Chroma-Key value of R channel
#define MKEY0_KEYG_SHIFT (8U)  // Chroma-Key value of G channel
#define MKEY0_KEYB_SHIFT (0U)  // Chroma-Key value of B channel

#define MKEY0_KEN_MASK   ((u32)0x1U << MKEY0_KEN_SHIFT)
#define MKEY0_KRYR_MASK  ((u32)0xFFU << MKEY0_KRYR_SHIFT)
#define MKEY0_KEYG_MASK  ((u32)0xFFU << MKEY0_KEYG_SHIFT)
#define MKEY0_KEYB_MASK  ((u32)0xFFU << MKEY0_KEYB_SHIFT)

/*
 * WMIX Image k Masked Chromak-key Register
 */
#define MKEY1_MKEYR_SHIFT (16U) // Masked Chroma-Key value of R channel
#define MKEY1_MKEYG_SHIFT (8U)  // Masked Chroma-Key value of G channel
#define MKEY1_MKEYB_SHIFT (0U)  // Masked Chroma-Key value of B channel

#define MKEY1_MKEYR_MASK ((u32)0xFFU << MKEY1_MKEYR_SHIFT)
#define MKEY1_MKEYG_MASK ((u32)0xFFU << MKEY1_MKEYG_SHIFT)
#define MKEY1_MKEYB_MASK ((u32)0xFFU << MKEY1_MKEYB_SHIFT)

/*
 * WMIX Status Register
 */
#define MSTS_DINTL_SHIFT (31U) // Download interlace status
#define MSTS_DBF_SHIFT   (30U) // Download Bfield status
#define MSTS_DEN_SHIFT   (29U) // Download Enable status
#define MSTS_DEOFW_SHIFT (28U) // Download EOF_WAIT status
#define MSTS_UINTL_SHIFT (27U) // Upload interlace status
#define MSTS_UUPD_SHIFT  (26U) // Upload update status
#define MSTS_UEN_SHIFT   (25U) // Upload enable status
#define MSTS_UEOF_SHIFT  (24U) // Upload EOF status
#define MSTS_EOFF_SHIFT  (4U)  // Falling EOF status
#define MSTS_EOFR_SHIFT  (3U)  // Rising EOF status
#define MSTS_EOFWR_SHIFT (2U)  // Rising EOF_WAIT status
#define MSTS_EOFWF_SHIFT (1U)  // Falling EOF_WAIT status
#define MSTS_UPD_SHIFT   (0U)  // Update Done

#define MSTS_DINTL_MASK  ((u32)0x1U << MSTS_DINTL_SHIFT)
#define MSTS_DBF_MASK    ((u32)0x1U << MSTS_DBF_SHIFT)
#define MSTS_DEN_MASK    ((u32)0x1U << MSTS_DEN_SHIFT)
#define MSTS_DEOFW_MASK  ((u32)0x1U << MSTS_DEOFW_SHIFT)
#define MSTS_UINTL_MASK  ((u32)0x1U << MSTS_UINTL_SHIFT)
#define MSTS_UUPD_MASK   ((u32)0x1U << MSTS_UUPD_SHIFT)
#define MSTS_UEN_MASK    ((u32)0x1U << MSTS_UEN_SHIFT)
#define MSTS_UEOF_MASK   ((u32)0x1U << MSTS_UEOF_SHIFT)
#define MSTS_EOFF_MASK   ((u32)0x1U << MSTS_EOFF_SHIFT)
#define MSTS_EOFR_MASK   ((u32)0x1U << MSTS_EOFR_SHIFT)
#define MSTS_EOFWR_MASK  ((u32)0x1U << MSTS_EOFWR_SHIFT)
#define MSTS_EOFWF_MASK  ((u32)0x1U << MSTS_EOFWF_SHIFT)
#define MSTS_UPD_MASK    ((u32)0x1U << MSTS_UPD_SHIFT)

/*
 * WMIX Interrupt Masked Register
 */
#define MIRQMSK_MEOFF_SHIFT  (4U) // Falling EOF Masked Interrupt
#define MIRQMSK_MEOFR_SHIFT  (3U) // Rising EOF Masked Interrupt
#define MIRQMSK_MEOFWR_SHIFT (2U) // Rising EOF_WAIT Masked Interrupt
#define MIRQMSK_MEOFWF_SHIFT (1U) // Falling EOF_WAIT Masked Interrupt
#define MIRQMSK_UPD_SHIFT    (0U) // Update done Masked Interrupt

#define MIRQMSK_MEOFF_MASK			((u32)0x1U << MIRQMSK_MEOFF_SHIFT)
#define MIRQMSK_MEOFR_MASK			((u32)0x1U << MIRQMSK_MEOFR_SHIFT)
#define MIRQMSK_MEOFWR_MASK			((u32)0x1U << MIRQMSK_MEOFWR_SHIFT)
#define MIRQMSK_MEOFF_MASK			((u32)0x1U << MIRQMSK_MEOFF_SHIFT)
#define MIRQMSK_UPD_MASK			((u32)0x1U << MIRQMSK_UPD_SHIFT)

/*
 * WMIX Alpha Control k Register
 */
#define MACON_ACON1_11_SHIFT		(28U) // ACON1 for Region C
#define MACON_ACON0_11_SHIFT		(24U) // ACON0 for Region C
#define MACON_ACON1_10_SHIFT		(20U) // ACON1 for Region B
#define MACON_ACON0_10_SHIFT		(16U) // ACON0 for Region B
#define MACON_ACON1_01_SHIFT		(12U) // ACON1 for Region D
#define MACON_ACON0_01_SHIFT		(8U)  // ACON0 for Region D
#define MACON_ACON1_00_SHIFT		(4U)  // ACON1 for Region A
#define MACON_ACON0_00_SHIFT		(0U)  // ACON0 for Region A

#define MACON_ACON1_11_MASK			((u32)0x7U << MACON_ACON1_11_SHIFT)
#define MACON_ACON0_11_MASK			((u32)0x7U << MACON_ACON0_11_SHIFT)
#define MACON_ACON1_10_MASK			((u32)0x7U << MACON_ACON1_10_SHIFT)
#define MACON_ACON0_10_MASK			((u32)0x7U << MACON_ACON0_10_SHIFT)
#define MACON_ACON1_01_MASK			((u32)0x7U << MACON_ACON1_01_SHIFT)
#define MACON_ACON0_01_MASK			((u32)0x7U << MACON_ACON0_01_SHIFT)
#define MACON_ACON1_00_MASK			((u32)0x7U << MACON_ACON1_00_SHIFT)
#define MACON_ACON0_00_MASK			((u32)0x7U << MACON_ACON0_00_SHIFT)

/*
 * WMIX Color Control k Register
 */
#define MCCON_CCON1_11_SHIFT		(28U) // ACON1 for Region C
#define MCCON_CCON0_11_SHIFT		(24U) // ACON0 for Region C
#define MCCON_CCON1_10_SHIFT		(20U) // ACON1 for Region B
#define MCCON_CCON0_10_SHIFT		(16U) // ACON0 for Region B
#define MCCON_CCON1_01_SHIFT		(12U) // ACON1 for Region D
#define MCCON_CCON0_01_SHIFT		(8U)  // ACON0 for Region D
#define MCCON_CCON1_00_SHIFT		(4U)  // ACON1 for Region A
#define MCCON_CCON0_00_SHIFT		(0U)  // ACON0 for Region A

#define MCCON_CCON1_11_MASK			((u32)0xFU << MCCON_CCON1_11_SHIFT)
#define MCCON_CCON0_11_MASK			((u32)0xFU << MCCON_CCON0_11_SHIFT)
#define MCCON_CCON1_10_MASK			((u32)0xFU << MCCON_CCON1_10_SHIFT)
#define MCCON_CCON0_10_MASK			((u32)0xFU << MCCON_CCON0_10_SHIFT)
#define MCCON_CCON1_01_MASK			((u32)0xFU << MCCON_CCON1_01_SHIFT)
#define MCCON_CCON0_01_MASK			((u32)0xFU << MCCON_CCON0_01_SHIFT)
#define MCCON_CCON1_00_MASK			((u32)0xFU << MCCON_CCON1_00_SHIFT)
#define MCCON_CCON0_00_MASK			((u32)0xFU << MCCON_CCON0_00_SHIFT)

/*
 * WMIX ROPk1 Control Register
 */
#define MROPC_ALPHA1_SHIFT   (24U)  // Alpha 1
#define MROPC_ALPHA0_SHIFT   (16U)  // Alpha 0
#define MROPC_ASEL_SHIFT     (14U)  // Alpha Selection
#define MROPC_ROPMODE_SHIFT  (0U)   // ROP Mode


#define MROPC_ALPHA1_MASK	((u32)0xFFU << MROPC_ALPHA1_SHIFT)
#define MROPC_ALPHA0_MASK	((u32)0xFFU << MROPC_ALPHA0_SHIFT)
#define MROPC_ASEL_MASK		((u32)0x3U << MROPC_ASEL_SHIFT)
#define MROPC_ROPMODE_MASK	((u32)0x1FU << MROPC_ROPMODE_SHIFT)

/*
 * WMIX ROP pattern k0 Register
 */
#define MPAT_RED_SHIFT    (16U)  // Red Color
#define MPAT_GREEN_SHIFT  (8U)   // Green color
#define MPAT_BLUE_SHIFT   (0U)   // Blue Color

#define MPAT_RED_MASK    ((u32)0xFFU << MPAT_RED_SHIFT)
#define MPAT_GREEN_MASK  ((u32)0xFFU << MPAT_GREEN_SHIFT)
#define MPAT_BLUE_MASK   ((u32)0xFFU << MPAT_BLUE_SHIFT)

/* Interface APIs */
extern void VIOC_WMIX_SetOverlayPriority(
	void __iomem *reg, unsigned int nOverlayPriority);
extern void VIOC_WMIX_GetOverlayPriority(
	const void __iomem *reg, unsigned int *nOverlayPriority);
extern int VIOC_WMIX_GetLayer(
	unsigned int vioc_id, unsigned int image_num);
extern int VIOC_WMIX_GetMixerType(unsigned int vioc_id);
extern void VIOC_WMIX_SetUpdate(void __iomem *reg);
extern void VIOC_WMIX_SetSize(void __iomem *reg,
	unsigned int nWidth, unsigned int nHeight);
extern void VIOC_WMIX_GetSize(const void __iomem *reg,
	unsigned int *nWidth, unsigned int *nHeight);
extern void VIOC_WMIX_SetBGColor(void __iomem *reg,
	unsigned int nBG0, unsigned int nBG1,
	unsigned int nBG2, unsigned int nBG3);
extern void VIOC_WMIX_SetPosition(void __iomem *reg,
	unsigned int nChannel, unsigned int nX, unsigned int nY);
extern void VIOC_WMIX_GetPosition(const void __iomem *reg,
	unsigned int nChannel, unsigned int *nX, unsigned int *nY);
extern void VIOC_WMIX_SetChromaKey(void __iomem *reg,
	unsigned int nLayer, unsigned int nKeyEn,
	unsigned int nKeyR, unsigned int nKeyG, unsigned int nKeyB,
	unsigned int nKeyMaskR, unsigned int nKeyMaskG, unsigned int nKeyMaskB);
extern void VIOC_WMIX_GetChromaKey(const void __iomem *reg,
	unsigned int nLayer, unsigned int *nKeyEn,
	unsigned int *nKeyR, unsigned int *nKeyG, unsigned int *nKeyB,
	unsigned int *nKeyMaskR, unsigned int *nKeyMaskG,
	unsigned int *nKeyMaskB);
extern void VIOC_WMIX_ALPHA_SetAlphaValueControl(
	void __iomem *reg, unsigned int layer,
	unsigned int region, unsigned int acon0, unsigned int acon1);
extern void VIOC_WMIX_ALPHA_SetColorControl(void __iomem *reg,
	unsigned int layer, unsigned int region,
	unsigned int ccon0, unsigned int ccon1);
extern void VIOC_WMIX_ALPHA_SetROPMode(void __iomem *reg,
	unsigned int layer, unsigned int mode);
extern void VIOC_WMIX_ALPHA_SetAlphaSelection(void __iomem *reg,
	unsigned int layer, unsigned int asel);
extern void VIOC_WMIX_ALPHA_SetAlphaValue(void __iomem *reg,
	unsigned int layer, unsigned int alpha0, unsigned int alpha1);
extern void VIOC_WMIX_ALPHA_SetROPPattern(void __iomem *reg,
	unsigned int layer, unsigned int patR,
	unsigned int patG, unsigned int patB);
extern void VIOC_WMIX_SetInterruptMask(void __iomem *reg,
	unsigned int nMask);
extern unsigned int VIOC_WMIX_GetStatus(const void __iomem *reg);
extern void __iomem *VIOC_WMIX_GetAddress(unsigned int vioc_id);
extern void VIOC_WMIX_DUMP(const void __iomem *reg, unsigned int vioc_id);

#endif
