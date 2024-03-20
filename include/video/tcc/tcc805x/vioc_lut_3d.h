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
#ifndef VIOC_3D_LUT_H
#define VIOC_3D_LUT_H

// 3D LUT Ctrl register
#define LUT_3D_CTRL_OFFSET (0x00U)

#define LUT_3D_CTRL_SEL_SHIFT       (28U)
#define LUT_3D_CTRL_SEL_MASK        ((u32)0xFU << LUT_3D_CTRL_SEL_SHIFT)
#define LUT_3D_CTRL_UPD_SHIFT       (16U)
#define LUT_3D_CTRL_UPD_MASK        ((u32)0x1U << LUT_3D_CTRL_UPD_SHIFT)
#define LUT_3D_CTRL_BYPASS_SHIFT    (1U)
#define LUT_3D_CTRL_BYPASS_MASK     ((u32)0x1U << LUT_3D_CTRL_BYPASS_SHIFT)
#define LUT_3D_CTRL_ENABLE_SHIFT    (0U)
#define LUT_3D_CTRL_ENABLE_MASK     ((u32)0x1U << LUT_3D_CTRL_ENABLE_SHIFT)

// 3D LUT Pend register
#define LUT_3D_PEND_OFFSET          ((u32)0x08U)

#define LUT_3D_PEND_PEND_SHIFT      (0U)
#define LUT_3D_PEND_PEND_MASK       ((u32)0x1U << LUT_3D_PEND_PEND_SHIFT)

// 3D LUT Table
#define LUT_3D_TABLE_OFFSET         ((u32)0x0CU)

extern int vioc_lut_3d_set_table(
	unsigned int lut_n,
	const unsigned int *lut3dtable);
extern int vioc_lut_3d_setupdate(
	unsigned int lut_n);
extern int vioc_lut_3d_bypass(
	unsigned int lut_n,
	unsigned int onoff);
extern int vioc_lut_3d_pend(
	unsigned int lut_n,
	unsigned int onoff);
extern int vioc_lut_3d_set_select(
	unsigned int lut_n,
	unsigned int sel);
extern void __iomem *get_lut_3d_address(
	unsigned int lut_n);

#endif
