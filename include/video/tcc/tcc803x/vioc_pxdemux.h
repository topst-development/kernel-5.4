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
#ifndef VIOC_PXDEMUX_H
#define	VIOC_PXDEMUX_H

#define PXDEMUX_TXOUT_MAX (8U)
#define PXDEMUX_TXOUT_(x) ((x) > PXDEMUX_TXOUT_MAX ? (-x) : (x))

/*
 * Register offset
 */
#define PD0_CFG		(0x040U)
#define PD1_CFG		(0x0C0U)
#define MUX3_1_EN0		(0x100U)
#define MUX3_1_SEL0		(0x104U)
#define MUX3_1_EN1		(0x108U)
#define MUX3_1_SEL1		(0x10CU)
#define MUX5_1_EN0		(0x110U)
#define MUX5_1_SEL0		(0x114U)
#define TXOUT_SEL0_0		(0x118U)
#define TXOUT_SEL1_0		(0x11CU)
#define TXOUT_SEL2_0		(0x120U)
#define TXOUT_SEL3_0		(0x124U)
#define TXOUT_SEL4_0		(0x128U)
#define TXOUT_SEL5_0		(0x12CU)
#define TXOUT_SEL6_0		(0x130U)
#define TXOUT_SEL7_0		(0x134U)
#define TXOUT_SEL8_0		(0x138U)
#define MUX5_1_EN1		(0x13CU)
#define MUX5_1_SEL1		(0x140U)
#define TXOUT_SEL0_1		(0x144U)
#define TXOUT_SEL1_1		(0x148U)
#define TXOUT_SEL2_1		(0x14CU)
#define TXOUT_SEL3_1		(0x150U)
#define TXOUT_SEL4_1		(0x154U)
#define TXOUT_SEL5_1		(0x158U)
#define TXOUT_SEL6_1		(0x15CU)
#define TXOUT_SEL7_1		(0x160U)
#define TXOUT_SEL8_1		(0x164U)
#define MUX5_1_EN2		(0x168U)
#define MUX5_1_SEL2		(0x16CU)
#define TXOUT_SEL0_2		(0x170U)
#define TXOUT_SEL1_2		(0x174U)
#define TXOUT_SEL2_2		(0x178U)
#define TXOUT_SEL3_2		(0x17CU)
#define TXOUT_SEL4_2		(0x180U)
#define TXOUT_SEL5_2		(0x184U)
#define TXOUT_SEL6_2		(0x188U)
#define TXOUT_SEL7_2		(0x18CU)
#define TXOUT_SEL8_2		(0x190U)
#define MUX5_1_EN3		(0x194U)
#define MUX5_1_SEL3		(0x198U)
#define TXOUT_SEL0_3		(0x19CU)
#define TXOUT_SEL1_3		(0x1A0U)
#define TXOUT_SEL2_3		(0x1A4U)
#define TXOUT_SEL3_3		(0x1A8U)
#define TXOUT_SEL4_3		(0x1ACU)
#define TXOUT_SEL5_3		(0x1B0U)
#define TXOUT_SEL6_3		(0x1B4U)
#define TXOUT_SEL7_3		(0x1B8U)
#define TXOUT_SEL8_3		(0x1BCU)

/*
 * Pixel demuxer configuration Register
 */
#define PD_CFG_WIDTH_SHIFT		(16U)
#define PD_CFG_SWAP3_SHIFT		(10U)
#define PD_CFG_SWAP2_SHIFT		(8U)
#define PD_CFG_SWAP1_SHIFT		(6U)
#define PD_CFG_SWAP0_SHIFT		(4U)
#define PD_CFG_MODE_SHIFT		(2U)
#define PD_CFG_LR_SHIFT		(1U)
#define PD_CFG_BP_SHIFT		(0U)

#define PD_CFG_WIDTH_MASK		((u32)0xFFFU << PD_CFG_WIDTH_SHIFT)
#define PD_CFG_SWAP3_MASK		((u32)0x3U << PD_CFG_SWAP3_SHIFT)
#define PD_CFG_SWAP2_MASK		((u32)0x3U << PD_CFG_SWAP2_SHIFT)
#define PD_CFG_SWAP1_MASK		((u32)0x3U << PD_CFG_SWAP1_SHIFT)
#define PD_CFG_SWAP0_MASK		((u32)0x3U << PD_CFG_SWAP0_SHIFT)
#define PD_CFG_MODE_MASK		((u32)0x1U << PD_CFG_MODE_SHIFT)
#define PD_CFG_LR_MASK		((u32)0x1U << PD_CFG_LR_SHIFT)
#define PD_CFG_BP_MASK		((u32)0x1U << PD_CFG_BP_SHIFT)

/*
 * Pixel demuxer MUX3to1 Enable Register
 */
#define MUX3_1_EN_EN_SHIFT		(0U)

#define MUX3_1_EN_EN_MASK		((u32)0x1U << MUX3_1_EN_EN_SHIFT)

/*
 * Pixel demuxer MUX3to1 Select Register
 */
#define MUX3_1_SEL_SEL_SHIFT		(0U)

#define MUX3_1_SEL_SEL_MASK		((u32)0x3U << MUX3_1_SEL_SEL_SHIFT)

/*
 * Pixel demuxer MUX5to1 Enable Register
 */
#define MUX5_1_EN_EN_SHIFT		(0U)

#define MUX5_1_EN_EN_MASK		((u32)0x1U << MUX5_1_EN_EN_SHIFT)

/*
 * Pixel demuxer MUX5to1 Select Register
 */
#define MUX5_1_SEL_SEL_SHIFT		(0U)

#define MUX5_1_SEL_SEL_MASK		((u32)0x7U << MUX5_1_SEL_SEL_SHIFT)

/*
 * Pixel demuxer TXOUT select0 Register
 */
#define TXOUT_SEL0_SEL03_SHIFT		(24U)
#define TXOUT_SEL0_SEL02_SHIFT		(16U)
#define TXOUT_SEL0_SEL01_SHIFT		(8U)
#define TXOUT_SEL0_SEL00_SHIFT		(0U)

#define TXOUT_SEL0_SEL03_MASK		((u32)0x1FU << TXOUT_SEL0_SEL03_SHIFT)
#define TXOUT_SEL0_SEL02_MASK		((u32)0x1FU << TXOUT_SEL0_SEL02_SHIFT)
#define TXOUT_SEL0_SEL01_MASK		((u32)0x1FU << TXOUT_SEL0_SEL01_SHIFT)
#define TXOUT_SEL0_SEL00_MASK		((u32)0x1FU << TXOUT_SEL0_SEL00_SHIFT)

/*
 * Pixel demuxer TXOUT select1 Register
 */
#define TXOUT_SEL1_SEL07_SHIFT		(24U)
#define TXOUT_SEL1_SEL06_SHIFT		(16U)
#define TXOUT_SEL1_SEL05_SHIFT		(8U)
#define TXOUT_SEL1_SEL04_SHIFT		(0U)

#define TXOUT_SEL1_SEL07_MASK		((u32)0x1FU << TXOUT_SEL1_SEL07_SHIFT)
#define TXOUT_SEL1_SEL06_MASK		((u32)0x1FU << TXOUT_SEL1_SEL06_SHIFT)
#define TXOUT_SEL1_SEL05_MASK		((u32)0x1FU << TXOUT_SEL1_SEL05_SHIFT)
#define TXOUT_SEL1_SEL04_MASK		((u32)0x1FU << TXOUT_SEL1_SEL04_SHIFT)

/*
 * Pixel demuxer TXOUT select2 Register
 */
#define TXOUT_SEL2_SEL11_SHIFT		(24U)
#define TXOUT_SEL2_SEL10_SHIFT		(16U)
#define TXOUT_SEL2_SEL09_SHIFT		(8U)
#define TXOUT_SEL2_SEL08_SHIFT		(0U)

#define TXOUT_SEL2_SEL11_MASK		((u32)0x1FU << TXOUT_SEL2_SEL11_SHIFT)
#define TXOUT_SEL2_SEL10_MASK		((u32)0x1FU << TXOUT_SEL2_SEL10_SHIFT)
#define TXOUT_SEL2_SEL09_MASK		((u32)0x1FU << TXOUT_SEL2_SEL09_SHIFT)
#define TXOUT_SEL2_SEL08_MASK		((u32)0x1FU << TXOUT_SEL2_SEL08_SHIFT)

/*
 * Pixel demuxer TXOUT select3 Register
 */
#define TXOUT_SEL3_SEL15_SHIFT		(24U)
#define TXOUT_SEL3_SEL14_SHIFT		(16U)
#define TXOUT_SEL3_SEL13_SHIFT		(8U)
#define TXOUT_SEL3_SEL12_SHIFT		(0U)

#define TXOUT_SEL3_SEL15_MASK		((u32)0x1FU << TXOUT_SEL3_SEL15_SHIFT)
#define TXOUT_SEL3_SEL14_MASK		((u32)0x1FU << TXOUT_SEL3_SEL14_SHIFT)
#define TXOUT_SEL3_SEL13_MASK		((u32)0x1FU << TXOUT_SEL3_SEL13_SHIFT)
#define TXOUT_SEL3_SEL12_MASK		((u32)0x1FU << TXOUT_SEL3_SEL12_SHIFT)

/*
 * Pixel demuxer TXOUT select4 Register
 */
#define TXOUT_SEL4_SEL19_SHIFT		(24U)
#define TXOUT_SEL4_SEL18_SHIFT		(16U)
#define TXOUT_SEL4_SEL17_SHIFT		(8U)
#define TXOUT_SEL4_SEL16_SHIFT		(0U)

#define TXOUT_SEL4_SEL19_MASK		((u32)0x1FU << TXOUT_SEL4_SEL19_SHIFT)
#define TXOUT_SEL4_SEL18_MASK		((u32)0x1FU << TXOUT_SEL4_SEL18_SHIFT)
#define TXOUT_SEL4_SEL17_MASK		((u32)0x1FU << TXOUT_SEL4_SEL17_SHIFT)
#define TXOUT_SEL4_SEL16_MASK		((u32)0x1FU << TXOUT_SEL4_SEL16_SHIFT)

/*
 * Pixel demuxer TXOUT select5 Register
 */
#define TXOUT_SEL5_SEL23_SHIFT		(24U)
#define TXOUT_SEL5_SEL22_SHIFT		(16U)
#define TXOUT_SEL5_SEL21_SHIFT		(8U)
#define TXOUT_SEL5_SEL20_SHIFT		(0U)

#define TXOUT_SEL5_SEL23_MASK		((u32)0x1FU << TXOUT_SEL5_SEL23_SHIFT)
#define TXOUT_SEL5_SEL22_MASK		((u32)0x1FU << TXOUT_SEL5_SEL22_SHIFT)
#define TXOUT_SEL5_SEL21_MASK		((u32)0x1FU << TXOUT_SEL5_SEL21_SHIFT)
#define TXOUT_SEL5_SEL20_MASK		((u32)0x1FU << TXOUT_SEL5_SEL20_SHIFT)

/*
 * Pixel demuxer TXOUT select6 Register
 */
#define TXOUT_SEL6_SEL27_SHIFT		(24U)
#define TXOUT_SEL6_SEL26_SHIFT		(16U)
#define TXOUT_SEL6_SEL25_SHIFT		(8U)
#define TXOUT_SEL6_SEL24_SHIFT		(0U)

#define TXOUT_SEL6_SEL27_MASK		((u32)0x1FU << TXOUT_SEL6_SEL27_SHIFT)
#define TXOUT_SEL6_SEL26_MASK		((u32)0x1FU << TXOUT_SEL6_SEL26_SHIFT)
#define TXOUT_SEL6_SEL25_MASK		((u32)0x1FU << TXOUT_SEL6_SEL25_SHIFT)
#define TXOUT_SEL6_SEL24_MASK		((u32)0x1FU << TXOUT_SEL6_SEL24_SHIFT)

/*
 * Pixel demuxer TXOUT select7 Register
 */
#define TXOUT_SEL7_SEL31_SHIFT		(24U)
#define TXOUT_SEL7_SEL30_SHIFT		(16U)
#define TXOUT_SEL7_SEL29_SHIFT		(8U)
#define TXOUT_SEL7_SEL28_SHIFT		(0U)

#define TXOUT_SEL7_SEL31_MASK		((u32)0x1FU << TXOUT_SEL7_SEL31_SHIFT)
#define TXOUT_SEL7_SEL30_MASK		((u32)0x1FU << TXOUT_SEL7_SEL30_SHIFT)
#define TXOUT_SEL7_SEL29_MASK		((u32)0x1FU << TXOUT_SEL7_SEL29_SHIFT)
#define TXOUT_SEL7_SEL28_MASK		((u32)0x1FU << TXOUT_SEL7_SEL28_SHIFT)

/*
 * Pixel demuxer TXOUT select7 Register
 */
#define TXOUT_SEL8_SEL34_SHIFT		(16U)
#define TXOUT_SEL8_SEL33_SHIFT		(8U)
#define TXOUT_SEL8_SEL32_SHIFT		(0U)

#define TXOUT_SEL8_SEL34_MASK		((u32)0x1FU << TXOUT_SEL8_SEL34_SHIFT)
#define TXOUT_SEL8_SEL33_MASK		((u32)0x1FU << TXOUT_SEL8_SEL33_SHIFT)
#define TXOUT_SEL8_SEL32_MASK		((u32)0x1FU << TXOUT_SEL8_SEL32_SHIFT)

typedef enum {
	PD_TXOUT_SEL0 = 0,
	PD_TXOUT_SEL1,
	PD_TXOUT_SEL2,
	PD_TXOUT_SEL3,
	PD_TXOUT_SEL4,
	PD_TXOUT_SEL5,
	PD_TXOUT_SEL6,
	PD_TXOUT_SEL7,
	PD_TXOUT_SEL8,
	PD_TXOUT_SEL_MAX
} PD_TXOUT_SEL;

typedef enum {
	PD_IDX_0 = 0,
	PD_IDX_1,
	PD_IDX_MAX
} PD_IDX;

typedef enum {
	PD_SWAP_CH0 = 0,
	PD_SWAP_CH1,
	PD_SWAP_CH2,
	PD_SWAP_CH3,
	PD_SWAP_CH_MAX
} PD_SWAP_CH;

typedef enum {
	PD_MUX3TO1_TYPE = 0,
	PD_MUX5TO1_TYPE,
	PD_MUX_TYPE_MAX
} PD_MUX_TYPE;

typedef enum {
	PD_MUX_SEL_DISP0 = 0,
	PD_MUX_SEL_DISP1,
	PD_MUX_SEL_DISP2,
	PD_MUX_SEL_DEMUX0,
	PD_MUX_SEL_MAX
} PD_MUX_SEL;

typedef enum {
	PD_MUX3TO1_IDX0 = 0,
	PD_MUX3TO1_IDX1,
	PD_MUX3TO1_IDX_MAX,
} PD_MUX3TO1_IDX;

typedef enum {
	PD_MUX5TO1_IDX0 = 0,
	PD_MUX5TO1_IDX1,
	PD_MUX5TO1_IDX2,
	PD_MUX5TO1_IDX3,
	PD_MUX5TO1_IDX_MAX,
} PD_MUX5TO1_IDX;

#define TXOUT_DUMMY			((u32)0x1F)
#define TXOUT_DE				(24)
#define TXOUT_HS				(25)
#define TXOUT_VS				(26)
#define TXOUT_R_D(x)			(x + 0x10)
#define TXOUT_G_D(x)			(x + 0x8)
#define TXOUT_B_D(x)			(x)

#define TXOUT_MAX_LINE			4U
#define TXOUT_DATA_PER_LINE		7U
#define TXOUT_GET_DATA(i) \
	((TXOUT_DATA_PER_LINE - 1U) \
	- ((i) % TXOUT_DATA_PER_LINE) \
	+ (TXOUT_DATA_PER_LINE * ((i) / TXOUT_DATA_PER_LINE)) \
	)

extern void VIOC_PXDEMUX_SetConfigure(
	unsigned int idx, unsigned int lr,
	unsigned int bypass, unsigned int width);
extern void VIOC_PXDEMUX_SetDataSwap(
	unsigned int idx, unsigned int ch, unsigned int set);
extern void VIOC_PXDEMUX_SetMuxOutput(
	PD_MUX_TYPE mux, unsigned int ch,
	unsigned int select, unsigned int enable);
extern void VIOC_PXDEMUX_SetDataPath(
	unsigned int ch, unsigned int data_path, unsigned int set);
extern void VIOC_PXDEMUX_SetDataArray(
	unsigned int ch,
	unsigned int data[TXOUT_MAX_LINE][TXOUT_DATA_PER_LINE]);
extern void __iomem *VIOC_PXDEMUX_GetAddress(void);

#endif /*__VIOC_PXDEMUX_H__*/
