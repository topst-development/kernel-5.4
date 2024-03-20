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
#ifndef VIOC_MC_H
#define VIOC_MC_H

typedef struct {
	unsigned short width;   /* Source Width*/
	unsigned short height;  /* Source Height*/
} mc_info_type;

/*
 * register offset
 */
#define MC_CTRL				(0x00U)
#define MC_OFS_BASE_Y		(0x04U)
#define MC_OFS_BASE_C		(0x08U)
#define MC_PIC				(0x0CU)
#define MC_FRM_STRIDE		(0x10U)
#define MC_FRM_BASE_Y		(0x14U)
#define MC_FRM_BASE_C		(0x18U)
#define MC_FRM_POS			(0x1CU)
#define MC_FRM_SIZE			(0x20U)
#define MC_FRM_MISC0		(0x24U)
#define MC_FRM_MISC1		(0x28U)
#define MC_TIMEOUT			(0x2CU)
#define MC_DITH_CTRL		(0x30U)
#define MC_DITH_MAT0		(0x34U)
#define MC_DITH_MAT1		(0x38U)
#define MC_OFS_RADDE_Y		(0x50U)
#define MC_OFS_RADDR_C		(0x54U)
#define MC_COM_RADDR_Y		(0x58U)
#define MC_COM_RADDR_C		(0x5CU)
#define MC_DMA_STAT			(0x60U)
#define MC_LBUF_CNT0		(0x64U)
#define MC_LBUF_CNT1		(0x68U)
#define MC_CBUF_CNT			(0x6CU)
#define MC_POS_STAT			(0x70U)
#define MC_STAT				(0x74U)
#define MC_IRQ				(0x78U)

/*
 * MC Control Register
 */
#define MC_CTRL_SWR_SHIFT   (31U) // SW Reset
#define MC_CTRL_Y2REN_SHIFT (23U) // Y2R Converter Enable
#define MC_CTRL_Y2RMD_SHIFT (20U) // Y2R Converter Mode
#define MC_CTRL_PAD_SHIFT   (17U) // Padding Register
#define MC_CTRL_UPD_SHIFT   (16U) // Information Update
#define MC_CTRL_SL_SHIFT    (8U)  // Start Line
#define MC_CTRL_DEPC_SHIFT  (6U)  // Bit Depth for Chroma
#define MC_CTRL_DEPY_SHIFT  (4U)  // Bit Depth for Luma
#define MC_CTRL_START_SHIFT (0U)  // Start flag

#define MC_CTRL_SWR_MASK   ((u32)0x1U << MC_CTRL_SWR_SHIFT)
#define MC_CTRL_Y2REN_MASK ((u32)0x1U << MC_CTRL_Y2REN_SHIFT)
#define MC_CTRL_Y2RMD_MASK ((u32)0x7U << MC_CTRL_Y2RMD_SHIFT)
#define MC_CTRL_PAD_MASK   ((u32)0x1U << MC_CTRL_PAD_SHIFT)
#define MC_CTRL_UPD_MASK   ((u32)0x1U << MC_CTRL_UPD_SHIFT)
#define MC_CTRL_SL_MASK    ((u32)0xFU << MC_CTRL_SL_SHIFT)
#define MC_CTRL_DEPC_MASK  ((u32)0x3U << MC_CTRL_DEPC_SHIFT)
#define MC_CTRL_DEPY_MASK  ((u32)0x3U << MC_CTRL_DEPY_SHIFT)
#define MC_CTRL_START_MASK ((u32)0x1U << MC_CTRL_START_SHIFT)

/*
 * MC Offset Table Luma Base Register
 */
#define MC_OFS_BASE_Y_BASE_SHIFT (0U)

#define MC_OFS_BASE_Y_BASE_MASK ((u32)0xFFFFFFFFU << MC_OFS_BASE_Y_BASE_SHIFT)

/*
 * MC Offset Table Chroma Base Register
 */
#define MC_OFS_BASE_C_BASE_SHIFT (0U)

#define MC_OFS_BASE_C_BASE_MASK ((u32)0xFFFFFFFFU << MC_OFS_BASE_C_BASE_SHIFT)

/*
 * MC Picture Height Register
 */
#define MC_PIC_HEIGHT_SHIFT (0U) // Picture Height

#define MC_PIC_HEIGHT_MASK ((u32)0x3FFFU << MC_PIC_HEIGHT_SHIFT)

/*
 * MC Compressed Data Strida Register
 */
#define MC_FRM_STRIDE_STRIDE_C_SHIFT (16U) // Chroma compressed data stride
#define MC_FRM_STRIDE_STRIDE_Y_SHIFT (0U)  // Luma compressed data stride

#define MC_FRM_STRIDE_STRIDE_C_MASK ((u32)0xFFFFU << MC_FRM_STRIDE_STRIDE_Y_SHIFT)
#define MC_FRM_STRIDE_STRIDE_Y_MASK ((u32)0xFFFFU << MC_FRM_STRIDE_STRIDE_Y_SHIFT)

/*
 * MC Compressed Data Luma Base Register
 */
#define MC_FRM_BASE_Y_BASE_SHIFT (0U)

#define MC_FRM_BASE_Y_BASE_MASK ((u32)0xFFFFFFFFU << MC_FRM_BASE_Y_BASE_SHIFT)

/*
 * MCCompressed Data Chroma Base Register
 */
#define MC_FRM_BASE_C_BASE_SHIFT (0U)

#define MC_RM_BASE_C_BASE_MASK ((u32)0xFFFFFFFFU << MC_FRM_BASE_C_BASE_SHIFT)

/*
 * MC position for Crop Register
 */
#define MC_FRM_POS_YPOS_SHIFT (16U) // vertical base position for crop image
#define MC_FRM_POS_XPOS_SHIFT (0U)  // horizontal base position for crop image

#define MC_FRM_POS_YPOS_MASK ((u32)0x1FFFU << MC_FRM_POS_YPOS_SHIFT)
#define MC_FRM_POS_XPOS_MASK ((u32)0x1FFFU << MC_FRM_POS_XPOS_SHIFT)

/*
 * MC Frame Size for Crop Register
 */
#define MC_FRM_SIZE_YSIZE_SHIFT (16U) // the vertical size for crop image
#define MC_FRM_SIZE_XSIZE_SHIFT (0U) // the horizontal size for crop image

#define MC_FRM_SIZE_YSIZE_MASK ((u32)0x3FFFU << MC_FRM_SIZE_YSIZE_SHIFT)
#define MC_FRM_SIZE_XSIZE_MASK ((u32)0x3FFFU << MC_FRM_SIZE_XSIZE_SHIFT)

/*
 * MC Miscellaneous Register
 */
#define MC_FRM_MISC0_HS_PERIOD_SHIFT   (16U) // period of horizontal sync pulse
#define MC_FRM_MISC0_COMP_ENDIAN_SHIFT (8U)  // Endian of compressed data DMA
#define MC_FRM_MISC0_OFS_ENDIAN_SHIFT  (4U)  // Endian for offset table DMA
#define MC_FRM_MISC0_OUT_MODE_SHIFT    (0U)  // output interface (DO NOT CHANGE)

#define MC_FRM_MISC0_HS_PERIOD_MASK   ((u32)0x3FFFU << MC_FRM_MISC0_HS_PERIOD_SHIFT)
#define MC_FRM_MISC0_COMP_ENDIAN_MASK ((u32)0xFU << MC_FRM_MISC0_COMP_ENDIAN_SHIFT)
#define MC_FRM_MISC0_OFS_ENDIAN_MASK  ((u32)0xFU << MC_FRM_MISC0_OFS_ENDIAN_SHIFT)
#define MC_FRM_MISC0_OUT_MODE_MASK    ((u32)0x7U << MC_FRM_MISC0_OUT_MODE_SHIFT)

/*
 * MC Miscellaneous Register (FOR DEBUG)
 */
#define MC_FRM_MISC1_ALPHA_SHIFT   (16U) // Set default alpha value
#define MC_FRM_MISC1_OUTMUX3_SHIFT (12U) // Select Mux3 Path
#define MC_FRM_MISC1_OUTMUX2_SHIFT (8U)  // Select Mux2 Path
#define MC_FRM_MISC1_OUTMUX1_SHIFT (4U)  // Select Mux1 Path
#define MC_FRM_MISC1_OUTMUX0_SHIFT (0U)  // Select Mux0 Path

#define MC_FRM_MISC1_ALPHA_MASK   ((u32)0xFFU << MC_FRM_MISC1_ALPHA_SHIFT)
#define MC_FRM_MISC1_OUTMUX3_MASK ((u32)0x7U << MC_FRM_MISC1_OUTMUX3_SHIFT)
#define MC_FRM_MISC1_OUTMUX2_MASK ((u32)0x7U << MC_FRM_MISC1_OUTMUX2_SHIFT)
#define MC_FRM_MISC1_OUTMUX1_MASK ((u32)0x7U << MC_FRM_MISC1_OUTMUX1_SHIFT)
#define MC_FRM_MISC1_OUTMUX0_MASK ((u32)0x7U << MC_FRM_MISC1_OUTMUX0_SHIFT)

/*
 * MC Timeout Register
 */
#define MC_TIMEOUT_TIMEOUT_SHIFT (0U)  // CHeck dead-lock mode (FOR DEBUG)

#define MC_TIMEOUT_TIMEOUT_MASK ((u32)0xFFFFU << MC_TIMEOUT_TIMEOUT_SHIFT)

/*
 * MC Dither Control Register
 */
#define MC_DITH_CTRL_MODE_SHIFT (12U)
#define MC_DITH_CTRL_SEL_SHIFT  (8U)
#define MC_DITH_CTRL_EN_SHIFT   (0U)

#define MC_DITH_CTRL_MODE_MASK ((u32)0x3U << MC_DITH_CTRL_MODE_SHIFT)
#define MC_DITH_CTRL_SEL_MASK  ((u32)0xFU << MC_DITH_CTRL_SEL_SHIFT)
#define MC_DITH_CTRL_EN_MASK   ((u32)0xFFU << MC_DITH_CTRL_EN_SHIFT)

/*
 * MC Dither Matrix Register 0
 */
#define MC_DITH_MAT0_DITH13_SHIFT (28U) // Dithering Pattern Matrix(1,3)
#define MC_DITH_MAT0_DITH12_SHIFT (24U) // Dithering Pattern Matrix(1,2)
#define MC_DITH_MAT0_DITH11_SHIFT (20U) // Dithering Pattern Matrix(1,1)
#define MC_DITH_MAT0_DITH10_SHIFT (16U) // Dithering Pattern Matrix(1,0)
#define MC_DITH_MAT0_DITH03_SHIFT (12U) // Dithering Pattern Matrix(0,3)
#define MC_DITH_MAT0_DITH02_SHIFT (8U)  // Dithering Pattern Matrix(0,2)
#define MC_DITH_MAT0_DITH01_SHIFT (4U)  // Dithering Pattern Matrix(0,1)
#define MC_DITH_MAT0_DITH00_SHIFT (0U)  // Dithering Pattern Matrix(0,0)

#define MC_DITH_MAT0_DITH13_MASK ((u32)0x7U << MC_DITH_MAT0_DITH13_SHIFT)
#define MC_DITH_MAT0_DITH12_MASK ((u32)0x7U << MC_DITH_MAT0_DITH12_SHIFT)
#define MC_DITH_MAT0_DITH11_MASK ((u32)0x7U << MC_DITH_MAT0_DITH11_SHIFT)
#define MC_DITH_MAT0_DITH10_MASK ((u32)0x7U << MC_DITH_MAT0_DITH10_SHIFT)
#define MC_DITH_MAT0_DITH03_MASK ((u32)0x7U << MC_DITH_MAT0_DITH03_SHIFT)
#define MC_DITH_MAT0_DITH02_MASK ((u32)0x7U << MC_DITH_MAT0_DITH02_SHIFT)
#define MC_DITH_MAT0_DITH01_MASK ((u32)0x7U << MC_DITH_MAT0_DITH01_SHIFT)
#define MC_DITH_MAT0_DITH00_MASK ((u32)0x7U << MC_DITH_MAT0_DITH00_SHIFT)

/*
 * MC Dither Matrix Register 1
 */
#define MC_DITH_MAT0_DITH33_SHIFT (28U) // Dithering Pattern Matrix(3,3)
#define MC_DITH_MAT0_DITH32_SHIFT (24U) // Dithering Pattern Matrix(3,2)
#define MC_DITH_MAT0_DITH31_SHIFT (20U) // Dithering Pattern Matrix(3,1)
#define MC_DITH_MAT0_DITH30_SHIFT (16U) // Dithering Pattern Matrix(3,0)
#define MC_DITH_MAT0_DITH23_SHIFT (12U) // Dithering Pattern Matrix(2,3)
#define MC_DITH_MAT0_DITH22_SHIFT (8U)  // Dithering Pattern Matrix(2,2)
#define MC_DITH_MAT0_DITH21_SHIFT (4U)  // Dithering Pattern Matrix(2,1)
#define MC_DITH_MAT0_DITH20_SHIFT (0U)  // Dithering Pattern Matrix(2,0)

#define MC_DITH_MAT0_DITH33_MASK ((u32)0x7U << MC_DITH_MAT0_DITH33_SHIFT)
#define MC_DITH_MAT0_DITH32_MASK ((u32)0x7U << MC_DITH_MAT0_DITH32_SHIFT)
#define MC_DITH_MAT0_DITH31_MASK ((u32)0x7U << MC_DITH_MAT0_DITH31_SHIFT)
#define MC_DITH_MAT0_DITH30_MASK ((u32)0x7U << MC_DITH_MAT0_DITH30_SHIFT)
#define MC_DITH_MAT0_DITH23_MASK ((u32)0x7U << MC_DITH_MAT0_DITH23_SHIFT)
#define MC_DITH_MAT0_DITH22_MASK ((u32)0x7U << MC_DITH_MAT0_DITH22_SHIFT)
#define MC_DITH_MAT0_DITH21_MASK ((u32)0x7U << MC_DITH_MAT0_DITH21_SHIFT)
#define MC_DITH_MAT0_DITH20_MASK ((u32)0x7U << MC_DITH_MAT0_DITH20_SHIFT)

/*
 * MC Luma Offset Table Last Address Register
 * the latest transferred Luma AXI address in offset table DMA
 */
#define MC_OFS_RADDR_Y_BASE_SHIFT (0U)
#define MC_OFS_RADDR_Y_BASE_MASK ((u32)0xFFFFFFFFU << MC_OFS_RADDR_Y_BASE_SHIFT)

/*
 * MC Chroma Offset Table Last Address Register
 * the latest transferred Chroma AXI address in offset table DMA
 */
#define MC_OFS_RADDR_C_BASE_SHIFT (0U)
#define MC_OFS_RADDR_C_BASE_MASK ((u32)0xFFFFFFFFU << MC_OFS_RADDR_C_BASE_SHIFT)

/*
 * MC Luma Comp. Last Address Register
 * the latest transferred Luma AXI address in compressed data DMA
 */
#define MC_COM_RADDR_Y_BASE_SHIFT (0U)
#define MC_COM_RADDR_Y_BASE_MASK ((u32)0xFFFFFFFFU << MC_COM_RADDR_Y_BASE_SHIFT)

/*
 * MC Chroma Comp. Last Address Register
 * the latest transferred Chroma AXI address in compressed data DMA
 */
#define MC_COM_RADDR_C_BASE_SHIFT (0U)
#define MC_COM_RADDR_C_BASE_MASK ((u32)0xFFFFFFFFU << MC_COM_RADDR_C_BASE_SHIFT)

/*
 * MC DMA status Register
 */
#define MC_DMA_STAT_CDMA_AXI_C_SHIFT	(28U)
#define MC_DMA_STAT_CDMA_C_SHIFT		(24U)
#define MC_DMA_STAT_CDMA_AXI_Y_SHIFT	(20U)
#define MC_DMA_STAT_CDMA_Y_SHIFT		(16U)
#define MC_DMA_STAT_ODMA_AXI_C_SHIFT	(12U)
#define MC_DMA_STAT_ODMA_C_SHIFT		(8U)
#define MC_DMA_STAT_ODMA_AXI_Y_SHIFT	(4U)
#define MC_DMA_STAT_ODMA_Y_SHIFT		(0U)

#define MC_DMA_STAT_CDMA_AXI_C_MASK ((u32)0x3U << MC_DMA_STAT_CDMA_AXI_C_SHIFT)
#define MC_DMA_STAT_CDMA_C_MASK     ((u32)0xFU << MC_DMA_STAT_CDMA_C_SHIFT)
#define MC_DMA_STAT_CDMA_AXI_Y_MASK ((u32)0x3U << MC_DMA_STAT_CDMA_AXI_Y_SHIFT)
#define MC_DMA_STAT_CDMA_Y_MASK     ((u32)0xFU << MC_DMA_STAT_CDMA_Y_SHIFT)
#define MC_DMA_STAT_ODMA_AXI_C_MASK ((u32)0x7U << MC_DMA_STAT_ODMA_AXI_C_SHIFT)
#define MC_DMA_STAT_ODMA_C_MASK     ((u32)0x3U << MC_DMA_STAT_ODMA_C_SHIFT)
#define MC_DMA_STAT_ODMA_AXI_Y_MASK ((u32)0x7U << MC_DMA_STAT_ODMA_AXI_Y_SHIFT)
#define MC_DMA_STAT_ODMA_Y_MASK     ((u32)0x3U << MC_DMA_STAT_ODMA_Y_SHIFT)

/*
 * MC Luma LineBuffer Count 0 Register
 * the number of luma data count in the luma line buffer1
 */
#define MC_LBUF_CNT0_YBUF_CNT1_SHIFT (16U)
#define MC_LBUF_CNT0_YBUF_CNT0_SHIFT (0U)

#define MC_LBUF_CNT0_YBUF_CNT1_MASK ((u32)0x3FFU << MC_LBUF_CNT0_YBUF_CNT1_SHIFT)
#define MC_LBUF_CNT0_YBUF_CNT0_MASK ((u32)0x3FFU << MC_LBUF_CNT0_YBUF_CNT0_SHIFT)

/*
 * MC Luma LineBuffer Count 1 Register
 * the number of luma data count in the luma line buffer3
 */
#define MC_LBUF_CNT0_YBUF_CNT3_SHIFT (16U)
#define MC_LBUF_CNT0_YBUF_CNT2_SHIFT (0U)

#define MC_LBUF_CNT0_YBUF_CNT3_MASK ((u32)0x3FFU << MC_LBUF_CNT0_YBUF_CNT3_SHIFT)
#define MC_LBUF_CNT0_YBUF_CNT2_MASK ((u32)0x3FFU << MC_LBUF_CNT0_YBUF_CNT2_SHIFT)

/*
 * MC Chroma LineBuffer Count 0 Register
 * the number of chroma data count in the luma line buffer1
 */
#define MC_CBUF_CNT_YBUF_CNT1_SHIFT (16U)
#define MC_CBUF_CNT_YBUF_CNT0_SHIFT (0U)

#define MC_CBUF_CNT_YBUF_CNT1_MASK ((u32)0x1FFU << MC_CBUF_CNT_YBUF_CNT1_SHIFT)
#define MC_CBUF_CNT_YBUF_CNT0_MASK ((u32)0x1FFU << MC_CBUF_CNT_YBUF_CNT0_SHIFT)

/*
 * MC Position Status Register
 */
#define MC_POS_STAT_LINE_CNT_SHIFT  (16U)
#define MC_POS_STAT_PIXEL_CNT_SHIFT (0U)

#define MC_POS_STAT_LINE_CNT_MASK ((u32)0xFFFFU << MC_POS_STAT_LINE_CNT_SHIFT)
#define MC_POS_STAT_PIXEL_CNT_MASK ((u32)0xFFFFU << MC_POS_STAT_PIXEL_CNT_SHIFT)

/*
 * MC Status Register
 */
#define MC_STAT_BM_C_SHIFT	(10U) // Chroma bit mode
#define MC_STAT_BM_Y_SHIFT	(8U)  // Luma bit mode
#define MC_STAT_UDR_SHIFT	(7U)  // Buffer underrun
#define MC_STAT_ERR_SHIFT	(6U)  // Error bit
#define MC_STAT_DONE_SHIFT	(5U)  // Done Flag
#define MC_STAT_BUSY_SHIFT	(4U)  // Buffer Flag
#define MC_STAT_TG_STAT_SHIFT	(0U)  // Status of DISP I/F

#define MC_STAT_BM_C_MASK ((u32)0x3U << MC_STAT_BM_C_SHIFT)
#define MC_STAT_BM_Y_MASK ((u32)0x3U << MC_STAT_BM_Y_SHIFT)
#define MC_STAT_UDR_MASK ((u32)0x1U << MC_STAT_UDR_SHIFT)
#define MC_STAT_ERR_MASK ((u32)0x1U << MC_STAT_ERR_SHIFT)
#define MC_STAT_DONE_MASK ((u32)0x1U << MC_STAT_DONE_SHIFT)
#define MC_STAT_BUSY_MASK ((u32)0x1U << MC_STAT_BUSY_SHIFT)
#define MC_STAT_TG_STAT_MASK ((u32)0x7U << MC_STAT_TG_STAT_SHIFT)

/*
 * MC IRQ Register
 */
#define MC_IRQ_MUDR_SHIFT	(19U) // Masked UDR Interrupt
#define MC_IRQ_MERR_SHIFT	(18U) // Masked ERR Interrupt
#define MC_IRQ_MUPD_SHIFT	(17U) // Masked UPD Interrupt
#define MC_IRQ_MMD_SHIFT	(16U) // Masked MC_DONE Interrupt
#define MC_IRQ_UDR_SHIFT	(3U)  // Under-run interrupt
#define MC_IRQ_ERR_SHIFT	(2U)  // Err flag interrupt
#define MC_IRQ_UPD_SHIFT	(1U)  // Update Done interrupt
#define MC_IRQ_MD_SHIFT		(0U)  // Map_conv Done interrupt

#define MC_IRQ_MUDR_MASK	((u32)0x1U << MC_IRQ_MUDR_SHIFT)
#define MC_IRQ_MERR_MASK	((u32)0x1U << MC_IRQ_MERR_SHIFT)
#define MC_IRQ_MUPD_MASK	((u32)0x1U << MC_IRQ_MUPD_SHIFT)
#define MC_IRQ_MMD_MASK		((u32)0x1U << MC_IRQ_MMD_SHIFT)
#define MC_IRQ_UDR_MASK		((u32)0x1U << MC_IRQ_UDR_SHIFT)
#define MC_IRQ_ERR_MASK		((u32)0x1U << MC_IRQ_ERR_SHIFT)
#define MC_IRQ_UPD_MASK		((u32)0x1U << MC_IRQ_UPD_SHIFT)
#define MC_IRQ_MD_MASK		((u32)0x1U << MC_IRQ_MD_SHIFT)

extern void VIOC_MC_Get_OnOff(
	const void __iomem *reg, uint *enable);
extern void VIOC_MC_Start_OnOff(
	void __iomem *reg, uint OnOff);
extern void VIOC_MC_UPD(void __iomem *reg);
extern void VIOC_MC_Y2R_OnOff(
	void __iomem *reg, uint OnOff, uint y2rmode);
extern void VIOC_MC_Start_BitDepth(
	void __iomem *reg, uint Chroma, uint Luma);
extern void VIOC_MC_OFFSET_BASE(
	void __iomem *reg, uint base_y, uint base_c);
extern void VIOC_MC_FRM_BASE(
	void __iomem *reg, uint base_y, uint base_c);
extern void VIOC_MC_FRM_SIZE(
	void __iomem *reg, uint xsize, uint ysize);
extern void VIOC_MC_FRM_SIZE_MISC(
	void __iomem *reg, uint pic_height,
	uint stride_y, uint stride_c);
extern void VIOC_MC_FRM_POS(
	void __iomem *reg, uint xpos, uint ypos);
extern void VIOC_MC_ENDIAN(
	void __iomem *reg, uint ofs_endian, uint comp_endian);
extern void VIOC_MC_DITH_CONT(
	void __iomem *reg, uint en, uint sel);
extern void VIOC_MC_SetDefaultAlpha(
	void __iomem *reg, uint alpha);
extern void __iomem *VIOC_MC_GetAddress(
	unsigned int vioc_id);
extern int  tvc_mc_get_info(
	unsigned int component_num, mc_info_type *pMC_info);
extern void VIOC_MC_DUMP(unsigned int vioc_id);

#endif
