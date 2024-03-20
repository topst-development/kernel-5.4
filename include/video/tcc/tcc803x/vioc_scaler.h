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
#ifndef VIOC_SCALER_H
#define	VIOC_SCALER_H

#define VIOC_SC_INT_MASK_UPDDONE (0x00000001U)
#define VIOC_SC_INT_MASK_EOFRISE (0x00000002U)
#define VIOC_SC_INT_MASK_EOFFALL (0x00000004U)
#define VIOC_SC_INT_MASK_ERROR (0x00000008U)
#define VIOC_SC_INT_MASK_ALL (0x0000000FU)

#define VIOC_SC_IREQ_UPDDONE_MASK (0x00000001U)
#define VIOC_SC_IREQ_EOFRISE_MASK (0x00000002U)
#define VIOC_SC_IREQ_EOFFALL_MASK (0x00000004U)
#define VIOC_SC_IREQ_ERROR_MASK   (0x00000008U)

/*
 * register offset
 */
#define SCCTRL   (0x00U)
#define SCSSIZE  (0x08U)
#define SCDSIZE  (0x0CU)
#define SCOPOS   (0x10U)
#define SCOSIZE  (0x14U)
#define SCIRQSTS (0x18U)
#define SCIRQMSK (0x1CU)

/*
 * Scaler Control Register
 */
#define SCCTRL_3DM_SHIFT (30U)
#define SCCTRL_UPD_SHIFT (16U)
#define SCCTRL_BP_SHIFT  (0U)

#define SCCTRL_3DM_MASK ((u32)0x3U << SCCTRL_3DM_SHIFT)
#define SCCTRL_UPD_MASK ((u32)0x1U << SCCTRL_UPD_SHIFT)
#define SCCTRL_BP_MASK  ((u32)0x1U << SCCTRL_BP_SHIFT)

/*
 * Scaler Source Size Register
 */
#define SCSSIZE_HEIGHT_SHIFT (16U)
#define SCSSIZE_WIDTH_SHIFT  (0U)

#define SCSSIZE_HEIGHT_MASK ((u32)0x1FFFU << SCSSIZE_HEIGHT_SHIFT)
#define SCSSIZE_WIDTH_MASK  ((u32)0x1FFFU << SCSSIZE_WIDTH_SHIFT)

/*
 * Scaler Destination Size Register
 */
#define SCDSIZE_HEIGHT_SHIFT (16U)
#define SCDSIZE_WIDTH_SHIFT  (0U)

#define SCDSIZE_HEIGHT_MASK ((u32)0x1FFFU << SCDSIZE_HEIGHT_SHIFT)
#define SCDSIZE_WIDTH_MASK  ((u32)0x1FFFU << SCDSIZE_WIDTH_SHIFT)

/*
 * Scaler Output Position Register
 */
#define SCOPOS_YPOS_SHIFT (16U)
#define SCOPOS_XPOS_SHIFT (0U)

#define SCOPOS_YPOS_MASK ((u32)0x1FFFU << SCOPOS_YPOS_SHIFT)
#define SCOPOS_XPOS_MASK ((u32)0x1FFFU << SCOPOS_XPOS_SHIFT)

/*
 * Scaler Output Size Register
 */
#define SCOSIZE_HEIGHT_SHIFT (16U)
#define SCOSIZE_WIDTH_SHIFT  (0U)

#define SCOSIZE_HEIGHT_MASK ((u32)0x1FFFU << SCOSIZE_HEIGHT_SHIFT)
#define SCOSIZE_WIDTH_MASK  ((u32)0x1FFFU << SCOSIZE_WIDTH_SHIFT)

/*
 * Scaler Interrupt Status Register
 */
#define SCIRQSTS_ERR_SHIFT  (3U)
#define SCIRQSTS_EOFF_SHIFT (2U)
#define SCIRQSTS_EOFR_SHIFT (1U)
#define SCIRQSTS_UPD_SHIFT  (0U)

#define SCIRQSTS_ERR_MASK  ((u32)0x1U << SCIRQSTS_ERR_SHIFT)
#define SCIRQSTS_EOFF_MASK ((u32)0x1U << SCIRQSTS_EOFF_SHIFT)
#define SCIRQSTS_EOFR_MASK ((u32)0x1U << SCIRQSTS_EOFR_SHIFT)
#define SCIRQSTS_UPD_MASK  ((u32)0x1U << SCIRQSTS_UPD_SHIFT)

/*
 * Scaler Interrupt Mask  Register
 */
#define SCIRQMSK_MERR_SHIFT  (3U)
#define SCIRQMSK_MEOFF_SHIFT (2U)
#define SCIRQMSK_MEOFR_SHIFT (1U)
#define SCIRQMSK_MUPD_SHIFT  (0U)

#define SCIRQMSK_MERR_MASK  ((u32)0x1U << SCIRQMSK_MERR_SHIFT)
#define SCIRQMSK_MEOFF_MASK ((u32)0x1U << SCIRQMSK_MEOFF_SHIFT)
#define SCIRQMSK_MEOFR_MASK ((u32)0x1U << SCIRQMSK_MEOFR_SHIFT)
#define SCIRQMSK_MUPD_MASK  ((u32)0x1U << SCIRQMSK_MUPD_SHIFT)

/* Interface APIs */
extern void VIOC_SC_SetBypass(void __iomem *reg,
	unsigned int nOnOff);
extern void VIOC_SC_SetUpdate(void __iomem *reg);
extern void VIOC_SC_SetSrcSize(void __iomem *reg,
	unsigned int nWidth, unsigned int nHeight);
extern void VIOC_SC_SetDstSize(void __iomem *reg,
	unsigned int nWidth, unsigned int nHeight);
extern void VIOC_SC_SetOutSize(void __iomem *reg,
	unsigned int nWidth, unsigned int nHeight);
extern void VIOC_SC_SetOutPosition(void __iomem *reg,
	unsigned int nXpos, unsigned int nYpos);
extern void __iomem *VIOC_SC_GetAddress(unsigned int vioc_id);
extern void VIOC_SCALER_DUMP(const void __iomem *reg,
	unsigned int vioc_id);

#endif
