/* SPDX-License-Identifier: GPL-2.0+ */
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
#ifndef TCC_LUT_H
#define TCC_LUT_H

extern int lut_drv_api_set_plugin(
	unsigned int lut_number,
	int plugin,
	int plug_in_ch);

extern int lut_drv_api_get_plugin(
	unsigned int lut_number);

extern int lut_drv_api_set_plugin_with_vioc_type(unsigned int lut_number, int plugin,
					  unsigned int plugComp);

#endif /* TCC_LUT_H*/
