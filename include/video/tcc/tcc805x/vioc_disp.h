/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * linux/arch/arm/mach-tcc893x/include/mach/vioc_disp.h
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

#ifndef VIOC_DISP_H
#define	VIOC_DISP_H

typedef struct {
	unsigned int nType;
	unsigned int CLKDIV;
	unsigned int IV;
	unsigned int IH;
	unsigned int IP;
	unsigned int DP;
	unsigned int NI;
	unsigned int TV;
	unsigned int LPW;
	unsigned int LPC;
	unsigned int LSWC;
	unsigned int LEWC;
	unsigned int FPW;
	unsigned int FLC;
	unsigned int FSWC;
	unsigned int FEWC;
	unsigned int FPW2;
	unsigned int FLC2;
	unsigned int FSWC2;
	unsigned int FEWC2;
} VIOC_TIMING_INFO;

/*
 * register offset
 */
#define	DCTRL		0x00U
#define DBC			0x04U
#define	DCLKDIV		0x08U
#define	DHTIME1		0x0CU
#define	DHTIME2		0x10U
#define DVTIME1		0x14U
#define DVTIME2		0x18U
#define DVTIME3		0x1CU
#define DVTIME4		0x20U
#define	DEFR		0x24U
#define DVTIME1_3D	0x28U
#define DVTIME2_3D	0x2CU
#define	DPOS		0x30U
#define DVTIME3_3D	0x34U
#define DVTIME4_3D	0x38U
#define	DBLK_VAL	0x3CU
#define DDITH		0x40U
#define DCPY		0x44U
#define DCPC		0x48U
#define	DDS			0x4CU
#define	DSTATUS		0x50U
#define	DIM			0x54U
#define	DALIGN		0x6CU
#define DCENH		0x70U
#define	DADVI		0x74U
#define DDMAT0		0x78U
#define DDMAT1		0x7CU

/*
 * Display Device Control register
 */
#define DCTRL_EVP_SHIFT    (31U) // External Vsync Polarity
#define DCTRL_EVS_SHIFT    (30U) // External Vsync Enable
#define DCTRL_R2YMD_SHIFT  (28U) // RGB to YCbCr Conversion Option
#define DCTRL_FLDINV_SHIFT (27U)	// Field Output Polarity Register
#define DCTRL_ADVI_SHIFT   (26U) // Advanced interlaced mode
#define DCTRL_656_SHIFT    (24U) // CCIR 656 Mode
#define DCTRL_CKG_SHIFT    (23U) // Clock Gating Enable for Timing Generator
#define DCTRL_SREQ_SHIFT   (22U) // Stop Request
#define DCTRL_Y2RMD_SHIFT  (20U) // YUV to RGB converter mode register
#define DCTRL_PXDW_SHIFT   (16U) // PXDW
#define DCTRL_ID_SHIFT     (15U) // Inverted Data Enable
#define DCTRL_IV_SHIFT     (14U) // Inverted Vertical Sync
#define DCTRL_IH_SHIFT     (13U) // Inverted Horizontal Sync
#define DCTRL_IP_SHIFT     (12U) // Inverted Pixel Clock
#define DCTRL_CLEN_SHIFT   (11U) // Clipping Enable
#define DCTRL_R2Y_SHIFT    (10U) // RGB to YCbCr Converter Enable for Output
#define DCTRL_DP_SHIFT     (9U)  // Double Pixel Data
#define DCTRL_NI_SHIFT     (8U)  // Non-Interlace
#define DCTRL_TV_SHIFT     (7U)  // TV mode
#define DCTRL_SRST_SHIFT   (6U)  // Device display reset
#define DCTRL_Y2R_SHIFT    (4U)  // YUV to RGB converter enable register
#define DCTRL_SWAPBF_SHIFT (1U)  // Output RGB overlay swap
#define DCTRL_LEN_SHIFT    (0U)  // LCD Controller Enable

#define DCTRL_EVP_MASK			((u32)0x1U << DCTRL_EVP_SHIFT)
#define DCTRL_EVS_MASK			((u32)0x1U << DCTRL_EVS_SHIFT)
#define DCTRL_R2YMD_MASK		((u32)0x3U << DCTRL_R2YMD_SHIFT)
#define DCTRL_FLDINV_MASK		((u32)0x1U << DCTRL_FLDINV_SHIFT)
#define DCTRL_ADVI_MASK			((u32)0x1U << DCTRL_ADVI_SHIFT)
#define DCTRL_656_MASK			((u32)0x1U << DCTRL_656_SHIFT)
#define DCTRL_CKG_MASK			((u32)0x1U << DCTRL_CKG_SHIFT)
#define DCTRL_SREQ_MASK			((u32)0x1U << DCTRL_SREQ_SHIFT)
#define DCTRL_Y2RMD_MASK		((u32)0x3U << DCTRL_Y2RMD_SHIFT)
#define DCTRL_PXDW_MASK			((u32)0xFU << DCTRL_PXDW_SHIFT)
#define DCTRL_ID_MASK			((u32)0x1U << DCTRL_ID_SHIFT)
#define DCTRL_IV_MASK			((u32)0x1U << DCTRL_IV_SHIFT)
#define DCTRL_IH_MASK			((u32)0x1U << DCTRL_IH_SHIFT)
#define DCTRL_IP_MASK			((u32)0x1U << DCTRL_IP_SHIFT)
#define DCTRL_CLEN_MASK			((u32)0x1U << DCTRL_CLEN_SHIFT)
#define DCTRL_R2Y_MASK			((u32)0x1U << DCTRL_R2Y_SHIFT)
#define DCTRL_DP_MASK			((u32)0x1U << DCTRL_DP_SHIFT)
#define DCTRL_NI_MASK			((u32)0x1U << DCTRL_NI_SHIFT)
#define DCTRL_TV_MASK			((u32)0x1U << DCTRL_TV_SHIFT)
#define DCTRL_SRST_MASK			((u32)0x1U << DCTRL_SRST_SHIFT)
#define DCTRL_Y2R_MASK			((u32)0x1U << DCTRL_Y2R_SHIFT)
#define DCTRL_SWAPBF_MASK		((u32)0x7U << DCTRL_SWAPBF_SHIFT)
#define DCTRL_LEN_MASK			((u32)0x1U << DCTRL_LEN_SHIFT)

/*
 * Display Device BackGround Color Register
 */
#define DBC_BG3_SHIFT (24U) // Background color 3 (Alpha)
#define DBC_BG2_SHIFT (16U) // Background color 2(Y/B)
#define DBC_BG1_SHIFT (8U)  // Background color 1(Cb/G)
#define DBC_BG0_SHIFT (0U)  // Background color 0(Cr/R)

#define DBC_BG3_MASK ((u32)0xFFU << DBC_BG3_SHIFT)
#define DBC_BG2_MASK ((u32)0xFFU << DBC_BG2_SHIFT)
#define DBC_BG1_MASK ((u32)0xFFU << DBC_BG1_SHIFT)
#define DBC_BG0_MASK ((u32)0xFFU << DBC_BG0_SHIFT)

/*
 * Display Device Clock Divider Register
 */
#define DCLKDIV_PXCLKDIV_SHIFT		(0U) // Pixel clock divider

#define DCLKDIV_PXCLKDIV_MASK		((u32)0xFFU << DCLKDIV_PXCLKDIV_SHIFT)

/*
 * Display Device Horizontal Timing Register 1
 */
#define DHTIME1_LPW_SHIFT (16U)  // Line pluse width
#define DHTIME1_LPC_SHIFT (0U)   // Line pluse count

#define DHTIME1_LPW_MASK			((u32)0x1FFU << DHTIME1_LPW_SHIFT)
#define DHTIME1_LPC_MASK			((u32)0x3FFFU << DHTIME1_LPC_SHIFT)

/*
 * Display Device Horizontal Timing Register 2
 */
#define DHTIME2_LSWC_SHIFT (16U) // Line start wait clock
#define DHTIME2_LEWC_SHIFT (0U)  // Line end wait clock

#define DHTIME2_LSWC_MASK			((u32)0x1FFU << DHTIME2_LSWC_SHIFT)
#define DHTIME2_LEWC_MASK			((u32)0xFFFU << DHTIME2_LEWC_SHIFT)

/*
 * Display Device Vertical Timing Register 1
 */
#define DVTIME1_VDB_SHIFT (27U)  // Back porch
#define DVTIME1_VDF_SHIFT (22U)  // Front porch
#define DVTIME1_FPW_SHIFT (16U)  // Frame pluse width
#define DVTIME1_FLC_SHIFT (0U)   // Frame line count

#define DVTIME1_VDB_MASK			((u32)0x1FU << DVTIME1_VDB_SHIFT)
#define DVTIME1_VDF_MASK			((u32)0xFU <<  DVTIME1_VDF_SHIFT)
#define DVTIME1_FPW_MASK			((u32)0x3FU << DVTIME1_FPW_SHIFT)
#define DVTIME1_FLC_MASK			((u32)0x3FFFU << DVTIME1_FLC_SHIFT)

/*
 * Display Device Vertical Timing Register 2
 */
#define DVTIME2_FSWC_SHIFT (16U) // Frame start wait clock
#define DVTIME2_FEWC_SHIFT (0U)  // Frame end wait clock

#define DVTIME2_FSWC_MASK ((u32)0x1FFU << DVTIME2_FSWC_SHIFT)
#define DVTIME2_FEWC_MASK ((u32)0x1FFU << DVTIME2_FEWC_SHIFT)

/*
 * Display Device Vertical Timing Register 3
 */
#define DVTIME3_FPW_SHIFT (16U) // Frame pluse width
#define DVTIME3_FLC_SHIFT (0U)  // Frame line count

#define DVTIME3_FPW_MASK ((u32)0x3FU << DVTIME3_FPW_SHIFT)
#define DVTIME3_FLC_MASK ((u32)0x3FFFU << DVTIME3_FLC_SHIFT)

/*
 * Display Device Vertical Timing Register 4
 */
#define DVTIME4_FSWC_SHIFT (16U) // Frame start wait clock
#define DVTIME4_FEWC_SHIFT (0U)  // Frame end wait clock

#define DVTIME4_FSWC_MASK ((u32)0x1FFU << DVTIME4_FSWC_SHIFT)
#define DVTIME4_FEWC_MASK ((u32)0x1FFU << DVTIME4_FEWC_SHIFT)

/*
 * Display Device Emergency Flag Register
 */
#define DEFR_EN_SHIFT		(31U) // Enable emergency flag
#define DEFR_MEN_SHIFT		(30U) // Mask emergency flag
#define DEFR_BM_SHIFT		(22U) // bfield mode (for debug)
#define DEFR_EOFM_SHIFT		(20U) // EOF mode (for debug)
#define DEFR_HDMIFLD_SHIFT	(18U) // HDMI_FIELD value (for debug)
#define DEFR_HDMIVS_SHIFT	(16U) // HDMI_VS valuse (for debug)
#define DEFR_STH_SHIFT		(4U)  // Threshold value in emergency flag
#define DEFR_CTH_SHIFT		(0U)  // Threshold value in clear emergency flag

#define DEFR_EN_MASK		((u32)0x1U << DEFR_EN_SHIFT)
#define DEFR_MEN_MASK		((u32)0x1U << DEFR_MEN_SHIFT)
#define DEFR_BM_MASK		((u32)0x1U << DEFR_BM_SHIFT)
#define DEFR_EOFM_MASK		((u32)0x3U << DEFR_EOFM_SHIFT)
#define DEFR_HDMIFLD_MASK	((u32)0x3U << DEFR_HDMIFLD_SHIFT)
#define DEFR_HDMIVS_MASK	((u32)0x3U << DEFR_HDMIVS_SHIFT)
#define DEFR_STH_MASK		((u32)0xFU << DEFR_STH_SHIFT)
#define DEFR_CTH_MASK		((u32)0xFU << DEFR_CTH_SHIFT)

/*
 * Display Device Vertical Timing Register 1 in 3D
 */
#define DVTIME1_3D_MD_SHIFT			(31U) // Enable Timeset3 mode
#define DVTIME1_3D_FPW_SHIFT		(16U) // Frame pluse width
#define DVTIME1_3D_FLC_SHIFT		(0U)  // Frame line count

#define DVTIME1_3D_MD_MASK			((u32)0x1U << DVTIME1_3D_MD_SHIFT)
#define DVTIME1_3D_FPW_MASK			((u32)0x3FU << DVTIME1_3D_FPW_SHIFT)
#define DVTIME1_3D_FLC_MASK			((u32)0x3FFFU << DVTIME1_3D_FLC_SHIFT)

/*
 * Display Device Vertical Timing Register 2 in 3D
 */
#define DVTIME2_3D_FSWC_SHIFT		(16U) // Frame start wait clock
#define DVTIME2_3D_FEWC_SHIFT		(0U)  // Frame end wait clock

#define DVTIME2_3D_FSWC_MASK		((u32)0x1FFU << DVTIME2_3D_FSWC_SHIFT)
#define DVTIME2_3D_FEWC_MASK		((u32)0x1FFU << DVTIME2_3D_FEWC_SHIFT)

/*
 * Display Device Position Register
 */
#define DPOS_YPOS_SHIFT				(16U) // Window Position Y
#define DPOS_XPOS_SHIFT				(0U)  // Window Position X

#define DPOS_YPOS_MASK				((u32)0xFFFFU << DPOS_YPOS_SHIFT)
#define DPOS_XPOS_MASK				((u32)0xFFFFU << DPOS_XPOS_SHIFT)

/*
 * Display Device Vertical Timing Register 3 in 3D
 */
#define DVTIME3_3D_MD_SHIFT			(31U) // Enable Timeset4 mode
#define DVTIME3_3D_FPW_SHIFT		(16U) // Frame pluse width
#define DVTIME3_3D_FLC_SHIFT		(0U)  // Frame line count

#define DVTIME3_3D_MD_MASK			((u32)0x1U << DVTIME3_3D_MD_SHIFT)
#define DVTIME3_3D_FPW_MASK			((u32)0x3FU << DVTIME3_3D_FPW_SHIFT)
#define DVTIME3_3D_FLC_MASK			((u32)0x3FFFU << DVTIME3_3D_FLC_SHIFT)

/*
 * Display Device Vertical Timing Register 4 in 3D
 */
#define DVTIME4_3D_FSWC_SHIFT		(16U) // Frame start wait clock
#define DVTIME4_3D_FEWC_SHIFT		(0U)  // Frame end wait clock

#define DVTIME4_3D_FSWC_MASK		((u32)0x1FFU << DVTIME4_3D_FSWC_SHIFT)
#define DVTIME4_3D_FEWC_MASK		((u32)0x1FFU << DVTIME4_3D_FEWC_SHIFT)

/*
 * Display Device Blank Value Register
 */
#define DBLK_VAL_BM_SHIFT			(31U) // Enable Blank mode
#define DBLK_VAL_VALUE_SHIFT		(0U)  // DataLine Value

#define DBLK_VAL_BM_MASK    ((u32)0x1U << DBLK_VAL_BM_SHIFT)
#define DBLK_VAL_VALUE_MASK ((u32)0xFFFFFFU << DBLK_VAL_VALUE_SHIFT)

/*
 * Display Device Dithering Control Register
 */
#define DDITH_DEN_SHIFT				(31U) // Dither Enable
#define DDITH_DSEL_SHIFT			(30U) // Dither Selection

#define DDITH_DEN_MASK				((u32)0x1U << DDITH_DEN_SHIFT)
#define DDITH_DSEL_MASK				((u32)0x1U << DDITH_DSEL_SHIFT)

/*
 * Display Device Clipping Register Y
 */
#define DCPY_CLPL_SHIFT				(16U) // Clipping Y/R below
#define DCPY_CLPH_SHIFT				(0U)  // Clipping Y/R upper

#define DCPY_CLPL_MASK				((u32)0xFFU <<  DCPY_CLPL_SHIFT)
#define DCPY_CLPH_MASK				((u32)0xFFU << DCPY_CLPH_SHIFT)

/*
 * Display Device Clipping Register C
 */
#define DCPC_CLPL_SHIFT (16U) // Clipping Chroma/G/B below
#define DCPC_CLPH_SHIFT (0U)  // Clipping Chroma/G/B upper

#define DCPC_CLPL_MASK				((u32)0xFFU <<  DCPC_CLPL_SHIFT)
#define DCPC_CLPH_MASK				((u32)0xFFU << DCPC_CLPH_SHIFT)

/*
 * Display Device CD Display Size Register
 */
#define DDS_VSIZE_SHIFT				(16U) // Vertical size
#define DDS_HSIZE_SHIFT				(0U)  // Horizontal size

#define DDS_VSIZE_MASK				((u32)0x1FFFU << DDS_VSIZE_SHIFT)
#define DDS_HSIZE_MASK				((u32)0x1FFFU << DDS_HSIZE_SHIFT)

/*
 * Display Device Status Register
 */
#define DSTATUS_VS_SHIFT     (31U) // vertical sync
#define DSTATUS_BUSY_SHIFT   (30U) // Busy status
#define DSTATUS_TFIELD_SHIFT (29U) // Top Field indicator
#define DSTATUS_DEOF_SHIFT   (28U) // Device EOF indicator
#define DSTATUS_STATUS_SHIFT (0U)  // Status

#define DSTATUS_VS_MASK     ((u32)0x1U << DSTATUS_VS_SHIFT)
#define DSTATUS_BUSY_MASK   ((u32)0x1U << DSTATUS_BUSY_SHIFT)
#define DSTATUS_TFIELD_MASK ((u32)0x1U << DSTATUS_TFIELD_SHIFT)
#define DSTATUS_DEOF_MASK   ((u32)0x1U << DSTATUS_DEOF_SHIFT)
#define DSTATUS_STATUS_MASK ((u32)0xFFFFU << DSTATUS_STATUS_SHIFT)

/*
 * Display Device Interrupt Masking Register
 */
#define DIM_MASK_SHIFT (0U)  // Interrupt Mask

#define DIM_MASK_MASK ((u32)0x3FU << DIM_MASK_SHIFT)


/*
 * Display Device Align Register
 */
#define DALIGN_SWAPAF_SHIFT (3U)  // swap RGB data line after pxdw
#define DALIGN_SWAPBF_SHIFT (0U)  // swap RGB data line before pxdw

#define DALIGN_SWAPAF_MASK		((u32)0x7U << DALIGN_SWAPAF_SHIFT)
#define DALIGN_SWAPBF_MASK		((u32)0x7U << DALIGN_SWAPBF_SHIFT)

/*
 * Display Device Color Enhancement Register
 */
#define DCENH_HEN_SHIFT      (24U)  // Hue calibration enable
#define DCENH_HUE_SHIFT      (16U)  // Huse calibration value
#define DCENH_BRIGHT_SHIFT   (8U)   // Brightness calibration value
#define DCENH_CONTRAST_SHIFT (0U)   // Contrast calibration value

#define DCENH_HEN_MASK      ((u32)0x1U << DCENH_HEN_SHIFT)
#define DCENH_HUE_MASK      ((u32)0xFFU << DCENH_HUE_SHIFT)
#define DCENH_BRIGHT_MASK   ((u32)0xFFU << DCENH_BRIGHT_SHIFT)
#define DCENH_CONTRAST_MASK ((u32)0xFFU << DCENH_CONTRAST_SHIFT)

/*
 * Display Device Advanced Interlacer Coefficient Register
 */
#define DADVI_COEFF1_SHIFT (4U)
#define DADVI_COEFF0_SHIFT (0U)

#define DADVI_COEFF1_MASK ((u32)0xFU << DADVI_COEFF1_SHIFT)
#define DADVI_COEFF0_MASK ((u32)0xFU << DADVI_COEFF0_SHIFT)

/*
 * Display Device Dither Matrix Register 0
 */
#define DDMAT0_DITH13_SHIFT (28U)  // Dithering Pattern Matrix (1,3)
#define DDMAT0_DITH12_SHIFT (24U)  // Dithering Pattern Matrix (1,2)
#define DDMAT0_DITH11_SHIFT (20U)  // Dithering Pattern Matrix (1,1)
#define DDMAT0_DITH10_SHIFT (16U)  // Dithering Pattern Matrix (1,0)
#define DDMAT0_DITH03_SHIFT (12U)  // Dithering Pattern Matrix (0,3)
#define DDMAT0_DITH02_SHIFT (8U)   // Dithering Pattern Matrix (0,2)
#define DDMAT0_DITH01_SHIFT (4U)   // Dithering Pattern Matrix (0,1)
#define DDMAT0_DITH00_SHIFT (0U)   // Dithering Pattern Matrix (0,0)

#define DDMAT0_DITH13_MASK ((u32)0x7U << DDMAT0_DITH13_SHIFT)
#define DDMAT0_DITH12_MASK ((u32)0x7U << DDMAT0_DITH12_SHIFT)
#define DDMAT0_DITH11_MASK ((u32)0x7U << DDMAT0_DITH11_SHIFT)
#define DDMAT0_DITH10_MASK ((u32)0x7U << DDMAT0_DITH10_SHIFT)
#define DDMAT0_DITH03_MASK ((u32)0x7U << DDMAT0_DITH03_SHIFT)
#define DDMAT0_DITH02_MASK ((u32)0x7U << DDMAT0_DITH02_SHIFT)
#define DDMAT0_DITH01_MASK ((u32)0x7U << DDMAT0_DITH01_SHIFT)
#define DDMAT0_DITH00_MASK ((u32)0x7U << DDMAT0_DITH00_SHIFT)

/*
 * Display Device Dither Matrix Register 1
 */
#define DDMAT1_DITH33_SHIFT (28U)  // Dithering Pattern Matrix (1,3)
#define DDMAT1_DITH32_SHIFT (24U)  // Dithering Pattern Matrix (1,2)
#define DDMAT1_DITH31_SHIFT (20U)  // Dithering Pattern Matrix (1,1)
#define DDMAT1_DITH30_SHIFT (16U)  // Dithering Pattern Matrix (1,0)
#define DDMAT1_DITH23_SHIFT (12U)  // Dithering Pattern Matrix (0,3)
#define DDMAT1_DITH22_SHIFT (8U)   // Dithering Pattern Matrix (0,2)
#define DDMAT1_DITH21_SHIFT (4U)   // Dithering Pattern Matrix (0,1)
#define DDMAT1_DITH20_SHIFT (0U)   // Dithering Pattern Matrix (0,0)

#define DDMAT1_DITH33_MASK ((u32)0x7U << DDMAT1_DITH33_SHIFT)
#define DDMAT1_DITH32_MASK ((u32)0x7U << DDMAT1_DITH32_SHIFT)
#define DDMAT1_DITH31_MASK ((u32)0x7U << DDMAT1_DITH31_SHIFT)
#define DDMAT1_DITH30_MASK ((u32)0x7U << DDMAT1_DITH30_SHIFT)
#define DDMAT1_DITH23_MASK ((u32)0x7U << DDMAT1_DITH23_SHIFT)
#define DDMAT1_DITH22_MASK ((u32)0x7U << DDMAT1_DITH22_SHIFT)
#define DDMAT1_DITH21_MASK ((u32)0x7U << DDMAT1_DITH21_SHIFT)
#define DDMAT1_DITH20_MASK ((u32)0x7U << DDMAT1_DITH20_SHIFT)

typedef struct LCDCDEFAULT {
	unsigned int evp;     // External VSYNC Polarity
	unsigned int evs;     // External VSYNC Enable
	unsigned int r2ymd;   // RGB to YCbCr Conversion Option
	unsigned int advi;    // Advanced Interlaced Mode
	unsigned int dtype;   // LCD DMA Type
	unsigned int gen;     // Gamma Correction Enable Bit
	unsigned int ccir656; // CCIR 656 Mode
	unsigned int ckg;     // Clock Gating Enable for Timing
	unsigned int bpp;     // Bit Per Pixel for STN-LCD
	unsigned int pxdw;    // Pixel Data Width
	unsigned int id;      // Inverted Data Enable
	unsigned int iv;      // Inverted Vertical Sync
	unsigned int ih;      // Inverted Horizontal Sync
	unsigned int ip;      // Inverted pixel Clock
	unsigned int clen;    // clipping Enable
	unsigned int r2y;     // RGB to YCbCr Converter Enable
	unsigned int dp;      // Double Pixel Data
	unsigned int ni;      // Non-Interlaced
	unsigned int tv;      // TV Mode
	unsigned int y2r;     // YUV to RGB Converter Enable
} stLCDCTR;

typedef struct LDCTIMING {
	// LHTIME1
	unsigned int lpw;	// Line Pulse Width, HSync width
	unsigned int lpc;	// Line Pulse Count, HActive width
	// LHTIME2
	unsigned int lswc;	// Line Start Wait Clock, HFront porch
	unsigned int lewc;	// Line End wait clock, HBack porch
	// LVTIME1
	unsigned int vdb;	// Back Porch Delay
	unsigned int vdf;	// Front Porch Delay
	unsigned int fpw;	// Frame Pulse Width, VSync Width
	unsigned int flc;	// Frame Line Count, VActive width
	// LVTIME2
	unsigned int fswc;	// Frame Start Wait Cycle
	unsigned int fewc;	// Frame End Wait Cycle
	// LVTIME3
	unsigned int fpw2;	// Frame Pulse Width,
	unsigned int flc2;	// Frame Line count,
	// LVTIME4
	unsigned int fswc2;	 // Frame Start Wait Cycle
	unsigned int fewc2;	 // Frame End Wait Cycle
} stLTIMING;

#define VIOC_DISP_IREQ_FU_MASK		0x00000001U /* fifo underrun */
#define VIOC_DISP_IREQ_VSR_MASK		0x00000002U /* VSYNC rising */
#define VIOC_DISP_IREQ_VSF_MASK		0x00000004U /* VSYNC falling */
#define VIOC_DISP_IREQ_RU_MASK		0x00000008U /* Register Update */
#define VIOC_DISP_IREQ_DD_MASK		0x00000010U /* Disable Done */
#define VIOC_DISP_IREQ_SREQ_MASK	0x00000020U /* Stop Request */

#define VIOC_DISP_IREQ_DEOF_MASK	0x10000000U
#define VIOC_DISP_IREQ_TFIELD_MASK	0x20000000U
#define VIOC_DISP_IREQ_BUSY_MASK	0x40000000U
#define VIOC_DISP_IREQ_VS_MASK		0x80000000U

enum {
	DCTRL_FMT_4BIT = 0,	//Only STN LCD
	DCTRL_FMT_8BIT,		//Only STN LCD
	DCTRL_FMT_8BIT_RGB_STRIPE,
	DCTRL_FMT_16BIT_RGB565,
	DCTRL_FMT_16BIT_RGB555,
	DCTRL_FMT_18BIT_RGB666,
	DCTRL_FMT_8BIT_YCBCR0,		//CB->Y ->CR ->Y
	DCTRL_FMT_8BIT_YCBCR1,		//CR->Y ->CB -> Y
	DCTRL_FMT_16BIT_YCBCR0,		//CB->Y ->CR ->Y
	DCTRL_FMT_16BIT_YCBCR1,		//CR->Y ->CB -> Y
	DCTRL_FMT_8BIT_RGB_DLETA0,
	DCTRL_FMT_8BIT_RGB_DLETA1 = 11,
	DCTRL_FMT_24BIT_RGB888,
	DCTRL_FMT_8BIT_RGB_DUMMY,
	DCTRL_FMT_16BIT_RGB666,
	DCTRL_FMT_16BIT_RGB888,
	DCTRL_FMT_10BIT_RGB_STRIPE,
	DCTRL_FMT_10BIT_RGB_DELTA0,
	DCTRL_FMT_10BIT_RGB_DELTA1,
	DCTRL_FMT_10BIT_YCBCR0,
	DCTRL_FMT_10BIT_YCBCR1,
	DCTRL_FMT_20BIT_YCBCR0 = 21,
	DCTRL_FMT_20BIT_YCBCR1,
	DCTRL_FMT_30BIT_RGB,
	DCTRL_FMT_10BIT_RGB_DUMMY,
	DCTRL_FMT_20BIT_RGB,
	DCTRL_FMT_24BIT_YCBCR,
	DCTRL_FMT_30BIT_YCBCR,
	DCTRL_FMT_MAX
};

typedef struct LCDC_PARAM {
	stLCDCTR LCDCTRL;
	stLTIMING LCDCTIMING;
} stLCDCPARAM;

struct DisplayBlock_Info {
	unsigned int enable;
	stLCDCTR pCtrlParam;
	unsigned int width;
	unsigned int height;
};

/* Interface APIs */
extern void VIOC_DISP_SetControlConfigure(
	void __iomem *reg, const stLCDCTR *pCtrlParam);
extern void VIOC_DISP_SetPXDW(
	void __iomem *reg, unsigned char PXDW);
extern void VIOC_DISP_SetR2YMD(
	void __iomem *reg, unsigned char R2YMD);
extern void VIOC_DISP_SetR2Y(
	void __iomem *reg, unsigned char R2Y);
extern void VIOC_DISP_SetY2RMD(
	void __iomem *reg, unsigned char Y2RMD);
extern void VIOC_DISP_SetY2R(
	void __iomem *reg, unsigned char Y2R);
extern void VIOC_DISP_SetSWAP(
	void __iomem *reg, unsigned char SWAP);
extern void VIOC_DISP_SetCKG(
	void __iomem *reg, unsigned char CKG);
extern void VIOC_DISP_SetSize(
	void __iomem *reg,
	unsigned int nWidth, unsigned int nHeight);
extern void VIOC_DISP_GetSize(
	const void __iomem *reg,
	unsigned int *nWidth, unsigned int *nHeight);
extern void VIOC_DISP_SetSwapbf(
	void __iomem *reg, unsigned int swapbf);
extern void VIOC_DISP_GetSwapbf(
	const void __iomem *reg, unsigned int *swapbf);
extern void VIOC_DISP_SetSwapaf(
	void __iomem *reg, unsigned int swapaf);
extern void VIOC_DISP_GetSwapaf(
	const void __iomem *reg, unsigned int *swapaf);

extern void VIOC_DISP_SetBGColor(
	void __iomem *reg,
	unsigned int BG0, unsigned int BG1, unsigned int BG2, unsigned int BG3);
extern void VIOC_DISP_SetPosition(
	void __iomem *reg, unsigned int startX, unsigned int startY);
extern void VIOC_DISP_GetPosition(
	const void __iomem *reg, unsigned int *startX, unsigned int *startY);
extern void VIOC_DISP_SetColorEnhancement(
	void __iomem *reg, unsigned int contrast,
	unsigned int brightness, unsigned int hue);
extern void VIOC_DISP_GetColorEnhancement(
	const void __iomem *reg, unsigned int *contrast,
	unsigned int *brightness, unsigned int *hue);
extern unsigned int VIOC_DISP_FMT_isRGB(unsigned int pxdw);
extern void VIOC_DISP_GetDisplayBlock_Info(
	const void __iomem *reg, struct DisplayBlock_Info *DDinfo);
extern void VIOC_DISP_SetClippingEnable(
	void __iomem *reg, unsigned int enable);
extern void VIOC_DISP_GetClippingEnable(
	const void __iomem *reg, unsigned int *enable);
extern void VIOC_DISP_SetClipping(
	void __iomem *reg,
	unsigned int uiUpperLimitY, unsigned int uiLowerLimitY,
	unsigned int uiUpperLimitUV, unsigned int uiLowerLimitUV);
extern void VIOC_DISP_GetClipping(
	const void __iomem *reg,
	unsigned int *uiUpperLimitY, unsigned int *uiLowerLimitY,
	unsigned int *uiUpperLimitUV, unsigned int *uiLowerLimitUV);
extern void VIOC_DISP_SetDither(
	void __iomem *reg,
	unsigned int ditherEn, unsigned int ditherSel,
	unsigned char mat[4][4]);
extern void VIOC_DISP_SetTimingParam(
	void __iomem *reg, const stLTIMING *pTimeParam);
//extern void VIOC_DISP_SetPixelClockDiv(
//	void __iomem *reg, stLTIMING *pTimeParam);
extern void VIOC_DISP_TurnOn(void __iomem *reg);
extern void VIOC_DISP_TurnOff(void __iomem *reg);
extern unsigned int  VIOC_DISP_Get_TurnOnOff(
	const void __iomem *reg);
extern int VIOC_DISP_Wait_DisplayDone(const void __iomem *reg);
extern int VIOC_DISP_sleep_DisplayDone(const void __iomem *reg);
extern void VIOC_DISP_SetControl(
	void __iomem *reg, const stLCDCPARAM *pLcdParam);
extern void VIOC_DISP_SetIreqMask(
	void __iomem *reg, unsigned int mask, unsigned int set);
extern void VIOC_DISP_SetStatus(
	void __iomem *reg, unsigned int set);
extern void VIOC_DISP_GetStatus(
	const void __iomem *reg, unsigned int *status);
extern void VIOC_DISP_EmergencyFlagDisable(
	void __iomem *reg);
extern void VIOC_DISP_EmergencyFlag_SetEofm(
	void __iomem *reg, unsigned int eofm);
extern void VIOC_DISP_EmergencyFlag_SetHdmiVs(
	void __iomem *reg, unsigned int hdmivs);
extern void __iomem *VIOC_DISP_GetAddress(unsigned int vioc_id);
unsigned int vioc_disp_get_clkdiv(const void __iomem *reg);
void vioc_disp_set_clkdiv(void __iomem *reg, unsigned int divide);
extern void VIOC_DISP_DUMP(
	const void __iomem *reg, unsigned int vioc_id);

#endif
