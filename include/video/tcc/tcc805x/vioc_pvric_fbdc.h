/*
 * Copyright (C) 2018 Telechips, Inc.
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
#ifndef VIOC_PVRIC_FBDC_H
#define	VIOC_PVRIC_FBDC_H

typedef enum {
	VIOC_PVRICCTRL_SWIZZ_ARGB = 0,
	VIOC_PVRICCTRL_SWIZZ_ARBG,
	VIOC_PVRICCTRL_SWIZZ_AGRB,
	VIOC_PVRICCTRL_SWIZZ_AGBR,
	VIOC_PVRICCTRL_SWIZZ_ABGR,
	VIOC_PVRICCTRL_SWIZZ_ABRG,
	VIOC_PVRICCTRL_SWIZZ_RGBA = 8,
	VIOC_PVRICCTRL_SWIZZ_RBGA,
	VIOC_PVRICCTRL_SWIZZ_GRBA,
	VIOC_PVRICCTRL_SWIZZ_GBRA,
	VIOC_PVRICCTRL_SWIZZ_BGRA,
	VIOC_PVRICCTRL_SWIZZ_BRGA,
} VIOC_PVRICCTRL_SWIZZ_MODE;

typedef enum {
	PVRICCTRL_FMT_ARGB8888 = 0x0C,
	PVRICCTRL_FMT_RGB888 = 0x3A,
} VIOC_PVRICCTRL_FMT_MODE;

typedef enum {
	VIOC_PVRICCTRL_TILE_TYPE_8X8 = 1,
	VIOC_PVRICCTRL_TILE_TYPE_16X4,
	VIOC_PVRICCTRL_TILE_TYPE_32X2,
} VIOC_PVRICCTRL_TILE_TYPE;

typedef enum {
	VIOC_PVRICSIZE_MAX_WIDTH_8X8 = 2048,
	VIOC_PVRICSIZE_MAX_WIDTH_16X4 = 4096,
	VIOC_PVRICSIZE_MAX_WIDTH_32X2 = 7860,
} VIOC_PVRICSIZE_MAX_WIDTH;

/*
 * Register offset
 */
#define PVRICCTRL   (0x00U)
#define PVRICSIZE   (0x0CU)
#define PVRICRADDR  (0x10U)
#define PVRICURTILE (0x14U)
#define PVRICOFFS   (0x18U)
#define PVRICOADDR  (0x1CU)
#define PVRICCCR0   (0x24U)
#define PVRICCCR1   (0x28U)
#define PVRICFSTAUS (0x34U)
#define PVRICIDLE   (0x74U)
#define PVRICSTS    (0x78U)

/*
 * PVRIC FBDC Control Register Information
 */
#define PVRICCTRL_SWIZZ_SHIFT     (24U)
#define PVRICCTRL_UPD_SHIFT       (16U)
#define PVRICCTRL_FMT_SHIFT       (8U)
#define PVRICCTRL_MEML_SHIFT      (7U)
#define PVRICCTRL_TILE_TYPE_SHIFT (5U)
#define PVRICCTRL_LOSSY_SHIFT     (4U)
#define PVRICCTRL_CONTEXT_SHIFT   (1U)
#define PVRICCTRL_START_SHIFT     (0U)

#define PVRICCTRL_SWIZZ_MASK     ((u32)0xFU << PVRICCTRL_SWIZZ_SHIFT)
#define PVRICCTRL_UPD_MASK       ((u32)0x1U << PVRICCTRL_UPD_SHIFT)
#define PVRICCTRL_FMT_MASK       ((u32)0x7FU << PVRICCTRL_FMT_SHIFT)
#define PVRICCTRL_TILE_TYPE_MASK ((u32)0x3U << PVRICCTRL_TILE_TYPE_SHIFT)
#define PVRICCTRL_LOSSY_MASK     ((u32)0x1U << PVRICCTRL_LOSSY_SHIFT)
#define PVRICCTRL_START_MASK     ((u32)0x1U << PVRICCTRL_START_SHIFT)

/*
 * PVRIC FBDC Frame Size Register Information
 */
#define PVRICSIZE_HEIGHT_SHIFT (16U)
#define PVRICSIZE_WIDTH_SHIFT  (0U)

#define PVRICSIZE_HEIGHT_MASK  ((u32)0x1FFFU << PVRICSIZE_HEIGHT_SHIFT)
#define PVRICSIZE_WIDTH_MASK   ((u32)0x1FFFU << PVRICSIZE_WIDTH_SHIFT)

/*
 * PVRIC FBDC Request Base address Register Information
 */
#define PVRICRADDR_SHIFT (0U)

#define PVRICRADDR_MASK  ((u32)0xFFFFFFFFU << PVRICRADDR_SHIFT)


/*
 * PVRIC FBDC Current Tile NumberRegister Information
 */
#define PVRICCURTILE_SHIFT (0U)

#define PVRICCURTILE_MASK  ((u32)0x7FFFFFU << PVRICCURTILE_SHIFT)


/*
 * PVRIC FBDC Output buffer Offset Register Information
 */
#define PVRICOFFS_SHIFT (0U)

#define PVRICOFFS_MASK  ((u32)0xFFFFFFFFU << PVRICOFFS_SHIFT)


/*
 * PVRIC FBDC Output buffer Base address Register Information
 */
#define PVRICOADDR_SHIFT (0U)

#define PVRICOADDR_MASK  ((u32)0xFFFFFFFFU << PVRICOADDR_SHIFT)


/*
 * PVRIC FBDC Constant Color Configuration Register
 */
#define PVRICCCR_CH3_PIXEL_SHIFT (24U)
#define PVRICCCR_CH2_PIXEL_SHIFT (16U)
#define PVRICCCR_CH1_PIXEL_SHIFT (8U)
#define PVRICCCR_CH0_PIXEL_SHIFT (0U)

#define PVRICCCR_CH3_PIXEL_MASK ((u32)0xFFU << PVRICCCR_CH3_PIXEL_SHIFT)
#define PVRICCCR_CH2_PIXEL_MASK ((u32)0xFFU << PVRICCCR_CH2_PIXEL_SHIFT)
#define PVRICCCR_CH1_PIXEL_MASK ((u32)0xFFU << PVRICCCR_CH1_PIXEL_SHIFT)
#define PVRICCCR_CH0_PIXEL_MASK ((u32)0xFFU << PVRICCCR_CH0_PIXEL_SHIFT)


/*
 * PVRIC FBDC Filter Status Register
 */
#define PVRICFSTAUS_CRF_ENABLE_MASK  ((u32)1U << PVRICFSTAUS_CRF_ENABLE_SHIFT)
#define PVRICFSTAUS_CRF_ENABLE_SHIFT 0U


/*
 * PVRIC FBDC Idle status Register Information
 */
#define PVRICIDLER_SHIFT (0U)

#define PVRICIDLER_MASK  ((u32)0x1U << PVRICIDLER_SHIFT)


/*
 * PVRIC FBDC IREQ Status Register Information
 */
#define PVRICSTS_IRQEN_EOF_ERR_SHIFT  (20U)
#define PVRICSTS_IRQEN_ADDR_ERR_SHIFT (19U)
#define PVRICSTS_IRQEN_TILE_ERR_SHIFT (18U)
#define PVRICSTS_IRQEN_UPD_SHIFT      (17U)
#define PVRICSTS_IRQEN_IDLE_SHIFT     (16U)

#define PVRICSTS_EOF_ERR_SHIFT  (4U)
#define PVRICSTS_ADDR_ERR_SHIFT (3U)
#define PVRICSTS_TILE_ERR_SHIFT (2U)
#define PVRICSTS_UPD_SHIFT      (1U)
#define PVRICSTS_IDLE_SHIFT     (0U)

#define PVRICSTS_IRQEN_EOF_ERR_MASK   ((u32)0x1U << PVRICSTS_IRQEN_EOF_ERR_SHIFT)
#define PVRICSTS_IRQEN_ADDR_ERR_MASK  ((u32)0x1U << PVRICSTS_IRQEN_ADDR_ERR_SHIFT)
#define PVRICSTS_IRQEN_TILE_ERR_MASK  ((u32)0x1U << PVRICSTS_IRQEN_TILE_ERR_SHIFT)
#define PVRICSTS_IRQEN_UPD_MASK       ((u32)0x1U << PVRICSTS_IRQEN_UPD_SHIFT)
#define PVRICSTS_IRQEN_IDLE_MASK      ((u32)0x1U << PVRICSTS_IRQEN_IDLE_SHIFT)
#define PVRICSTS_EOF_ERR_MASK   ((u32)0x1U << PVRICSTS_EOF_ERR_SHIFT)
#define PVRICSTS_ADDR_ERR_MASK  ((u32)0x1U << PVRICSTS_ADDR_ERR_SHIFT)
#define PVRICSTS_TILE_ERR_MASK  ((u32)0x1U << PVRICSTS_TILE_ERR_SHIFT)
#define PVRICSTS_UPD_MASK       ((u32)0x1U << PVRICSTS_UPD_SHIFT)
#define PVRICSTS_IDLE_MASK      ((u32)0x1U << PVRICSTS_IDLE_SHIFT)

#define PVRICSYS_IRQ_ALL \
	((PVRICSTS_EOF_ERR_MASK) \
	| (PVRICSTS_ADDR_ERR_MASK) \
	| (PVRICSTS_TILE_ERR_MASK) \
	| (PVRICSTS_UPD_MASK) \
	| (PVRICSTS_IDLE_MASK))
#define PVRICSYS_IRQEN_IRQ_ALL \
	((PVRICSTS_IRQEN_EOF_ERR_MASK) \
	| (PVRICSTS_IRQEN_ADDR_ERR_MASK) \
	| (PVRICSTS_IRQEN_TILE_ERR_MASK) \
	| (PVRICSTS_IRQEN_UPD_MASK) \
	| (PVRICSTS_IRQEN_IDLE_MASK))

/* Interface APIs */
extern void VIOC_PVRIC_FBDC_SetARGBSwizzMode(
	void __iomem *reg,
	VIOC_PVRICCTRL_SWIZZ_MODE mode);
extern void VIOC_PVRIC_FBDC_SetUpdateInfo(
	void __iomem *reg,
	unsigned int enable);
extern void VIOC_PVRIC_FBDC_SetFormat(
	void __iomem *reg,
	VIOC_PVRICCTRL_FMT_MODE fmt);
extern void VIOC_PVRIC_FBDC_SetTileType(
	void __iomem *reg,
	VIOC_PVRICCTRL_TILE_TYPE type);
extern void VIOC_PVRIC_FBDC_SetLossyDecomp(
	void __iomem *reg,
	unsigned int enable);

extern void VIOC_PVRIC_FBDC_SetFrameSize(
	void __iomem *reg,
	unsigned int width,
	unsigned int height);
extern void VIOC_PVRIC_FBDC_GetFrameSize(
	const void __iomem *reg,
	unsigned int *width,
	unsigned int *height);
extern void VIOC_PVRIC_FBDC_SetRequestBase(
	void __iomem *reg,
	unsigned int base);
extern void VIOC_PVRIC_FBDC_GetCurTileNum(
	const void __iomem *reg,
	unsigned int *tile_num);
extern void VIOC_PVRIC_FBDC_SetOutBufOffset(
	void __iomem *reg,
	unsigned int imgFmt,
	unsigned int imgWidth);
extern void VIOC_PVRIC_FBDC_SetOutBufBase(
	void __iomem *reg,
	unsigned int base);
extern void vioc_pvric_fbdc_set_val0_cr_ch0123(
	void __iomem *reg,
	unsigned int ch0, unsigned int ch1,
	unsigned int ch2, unsigned int ch3);
extern void vioc_pvric_fbdc_set_val1_cr_ch0123(
	void __iomem *reg,
	unsigned int ch0, unsigned int ch1,
	unsigned int ch2, unsigned int ch3);
extern void vioc_pvric_fbdc_set_cr_filter_enable(
	void __iomem *reg);
extern void vioc_pvric_fbdc_set_cr_filter_disable(
	void __iomem *reg);
extern unsigned int VIOC_PVRIC_FBDC_GetIdle(
	const void __iomem *reg);
extern unsigned int VIOC_PVRIC_FBDC_GetStatus(
	const void __iomem *reg);
extern void VIOC_PVRIC_FBDC_SetIrqMask(
	void __iomem *reg,
	unsigned int enable,
	unsigned int mask);
extern void VIOC_PVRIC_FBDC_ClearIrq(
	void __iomem *reg,
	unsigned int mask);
extern void VIOC_PVRIC_FBDC_TurnOFF(
	void __iomem *reg);
extern void VIOC_PVRIC_FBDC_TurnOn(
	void __iomem *reg);
extern unsigned int VIOC_PVRIC_FBDC_isTurnOnOff(
	const void __iomem *reg);

extern int VIOC_PVRIC_FBDC_SetBasicConfiguration(
	void __iomem *reg,
	unsigned int base,
	unsigned int imgFmt,
	unsigned int imgWidth,
	unsigned int imgHeight,
	unsigned int decomp_mode);
extern void VIOC_PVRIC_FBDC_DUMP(
	const void __iomem *reg,
	unsigned int vioc_id);
extern void __iomem *VIOC_PVRIC_FBDC_GetAddress(
	unsigned int vioc_id);

#endif
