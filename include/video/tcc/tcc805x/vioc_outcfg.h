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
#ifndef VIOC_OUTCFG_H
#define	VIOC_OUTCFG_H

#define VIOC_OUTCFG_HDMI 0
#define VIOC_OUTCFG_SDVENC 1
#define VIOC_OUTCFG_HDVENC 2
#define VIOC_OUTCFG_M80 3
#define VIOC_OUTCFG_MRGB 4

#define VIOC_OUTCFG_DISP0			0
#define VIOC_OUTCFG_DISP1			1
#define VIOC_OUTCFG_DISP2			2
#define VIOC_OUTCFG_V_DV			3

/*
 * Register offset
 */
#define D0CPUIF			(0x00U)
#define D1CPUIF			(0x04U)
#define D2CPUIF			(0x08U)
#define MISC			(0x0CU)

/*
 * CPUIFk Control Register
 */
#define DCPUIF_ID_SHIFT		(15U)
#define DCPUIF_IV_SHIFT		(14U)
#define DCPUIF_EN_SHIFT		(9U)
#define DCPUIF_SI_SHIFT		(8U)
#define DCPUIF_CS_SHIFT		(7U)
#define DCPUIF_XA_SHIFT		(5U)
#define DCPUIF_FMT_SHIFT	(0U)

#define DCPUIF_ID_MASK		((u32)0x1U << DCPUIF_ID_SHIFT)
#define DCPUIF_IV_MASK		((u32)0x1U << DCPUIF_IV_SHIFT)
#define DCPUIF_EN_MASK		((u32)0x1U << DCPUIF_EN_SHIFT)
#define DCPUIF_SI_MASK		((u32)0x1U << DCPUIF_SI_SHIFT)
#define DCPUIF_CS_MASK		((u32)0x1U << DCPUIF_CS_SHIFT)
#define DCPUIF_XA_MASK		((u32)0x3U << DCPUIF_XA_SHIFT)
#define DCPUIF_FMT_MASK		((u32)0xFU << DCPUIF_FMT_SHIFT)

/*
 * Miscellaneous Register
 */
#define MISC_MRGBSEL_SHIFT		(16U)
#define MISC_M80SEL_SHIFT		(12U)
#define MISC_HDVESEL_SHIFT		(8U)
#define MISC_SDVESEL_SHIFT		(4U)
#define MISC_HDMISEL_SHIFT		(0U)

#define MISC_MRGBSEL_MASK		((u32)0x3U << MISC_MRGBSEL_SHIFT)
#define MISC_M80SEL_MASK		((u32)0x3U << MISC_M80SEL_SHIFT)
#define MISC_HDVESEL_MASK		((u32)0x3U << MISC_HDVESEL_SHIFT)
#define MISC_SDVESEL_MASK		((u32)0x3U << MISC_SDVESEL_SHIFT)
#define MISC_HDMISEL_MASK		((u32)0x3U << MISC_HDMISEL_SHIFT)

extern void VIOC_OUTCFG_SetOutConfig(
	unsigned int nType, unsigned int nDisp);
extern void __iomem *VIOC_OUTCONFIG_GetAddress(void);
extern void VIOC_OUTCONFIG_DUMP(void);

#endif
